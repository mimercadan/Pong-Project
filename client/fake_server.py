"""Servidor de PRÁCTICA en Python que habla MyAppGameProtocol.

No es el PongServer (ese es el de C en server/). Sirve para probar el
cliente mientras el servidor real termina de integrarse, y como referencia
de qué debe responder el servidor a cada mensaje.

Uso: python3 fake_server.py [PUERTO]     (por defecto 5000)
"""
import socket
import sys
import threading
import time

import protocol as p

WIDTH, HEIGHT = 800, 600
PADDLE_H, PADDLE_W, MARGIN = 100, 20, 30
BALL_R, PADDLE_SPEED, WIN_SCORE = 10, 300, 5
TICK = 1 / 60

lock = threading.Lock()
players = {}      # id -> {"sock", "nick", "match"}
waiting = []      # ids esperando rival
next_id = 1
next_match = 1


def log(msg):
    print(time.strftime("[%H:%M:%S]"), msg, flush=True)


def send(pid, msg_type, payload=b""):
    try:
        p.send_message(players[pid]["sock"], msg_type, payload)
        if msg_type != p.STATE:
            log(f"SEND -> #{pid} {p.TYPE_NAMES[msg_type]}")
    except (OSError, KeyError):
        pass


class Match:
    def __init__(self, mid, left, right):
        self.id, self.left, self.right = mid, left, right
        self.ly = self.ry = (HEIGHT - PADDLE_H) / 2
        self.dir = {left: 0, right: 0}
        self.sl = self.sr = 0
        self.reset_ball(1)
        self.over = False

    def reset_ball(self, direction):
        self.bx, self.by = WIDTH / 2, HEIGHT / 2
        self.vx, self.vy = 300 * direction, 150

    def step(self, dt):
        self.ly = min(max(self.ly + self.dir[self.left] * PADDLE_SPEED * dt, 0), HEIGHT - PADDLE_H)
        self.ry = min(max(self.ry + self.dir[self.right] * PADDLE_SPEED * dt, 0), HEIGHT - PADDLE_H)
        self.bx += self.vx * dt
        self.by += self.vy * dt
        if self.by < BALL_R or self.by > HEIGHT - BALL_R:
            self.vy = -self.vy
            self.by = min(max(self.by, BALL_R), HEIGHT - BALL_R)
        if self.vx < 0 and self.bx - BALL_R <= MARGIN + PADDLE_W and self.ly <= self.by <= self.ly + PADDLE_H:
            self.vx = -self.vx
        if self.vx > 0 and self.bx + BALL_R >= WIDTH - MARGIN - PADDLE_W and self.ry <= self.by <= self.ry + PADDLE_H:
            self.vx = -self.vx
        if self.bx < 0:
            self.sr += 1
            self.reset_ball(1)
        elif self.bx > WIDTH:
            self.sl += 1
            self.reset_ball(-1)

    def state(self):
        return p.pack_state(self.bx, self.by, self.ly, self.ry, self.sl, self.sr)

    def finish(self, winner, reason):
        self.over = True
        payload = p.pack_game_over(winner, self.sl, self.sr, reason)
        for pid in (self.left, self.right):
            if pid in players:
                players[pid]["match"] = None
                send(pid, p.GAME_OVER, payload)
        log(f"Partida {self.id} terminó {self.sl}-{self.sr}, ganó #{winner}")


def run_match(match):
    while True:
        time.sleep(TICK)
        with lock:
            if match.over:
                return
            match.step(TICK)
            payload = match.state()
            for pid in (match.left, match.right):
                send(pid, p.STATE, payload)
            if match.sl >= WIN_SCORE or match.sr >= WIN_SCORE:
                match.finish(match.left if match.sl > match.sr else match.right, p.END_SCORE)
                return


def handle(sock, pid):
    global next_match
    registered = False
    try:
        while True:
            msg_type, payload = p.recv_message(sock)
            log(f"RECV <- #{pid} {p.TYPE_NAMES.get(msg_type, msg_type)} ({len(payload)} B)")
            with lock:
                me = players[pid]
                if msg_type == p.PING:
                    continue
                if msg_type == p.QUIT:
                    break
                if msg_type == p.REGISTER:
                    try:
                        nick, email = p.unpack_register(payload)
                    except p.ProtocolError:
                        send(pid, p.ERROR, p.pack_error(p.ERR_BAD_MESSAGE))
                        continue
                    if registered:
                        send(pid, p.REGISTER_RESULT, p.pack_register_result(p.REG_TAKEN, 0))
                    elif " " in nick or email.count("@") != 1:
                        send(pid, p.REGISTER_RESULT, p.pack_register_result(p.REG_INVALID, 0))
                    else:
                        registered, me["nick"] = True, nick
                        log(f"#{pid} registrado como {nick} <{email}>")
                        send(pid, p.REGISTER_RESULT, p.pack_register_result(p.REG_OK, pid))
                elif not registered:
                    send(pid, p.ERROR, p.pack_error(p.ERR_NOT_REGISTERED))
                elif msg_type == p.JOIN:
                    if me["match"] or pid in waiting:
                        send(pid, p.ERROR, p.pack_error(p.ERR_INVALID_STATE))
                    elif waiting:
                        rival = waiting.pop(0)
                        match = Match(next_match, rival, pid)
                        next_match += 1
                        me["match"] = players[rival]["match"] = match
                        send(rival, p.MATCH_START, p.pack_match_start(match.id, p.SIDE_LEFT, me["nick"]))
                        send(pid, p.MATCH_START, p.pack_match_start(match.id, p.SIDE_RIGHT, players[rival]["nick"]))
                        threading.Thread(target=run_match, args=(match,), daemon=True).start()
                    else:
                        waiting.append(pid)
                        send(pid, p.WAITING)
                elif msg_type == p.MOVE:
                    try:
                        direction = {p.MOVE_STOP: 0, p.MOVE_UP: -1, p.MOVE_DOWN: 1}[p.unpack_move(payload)]
                    except p.ProtocolError:
                        send(pid, p.ERROR, p.pack_error(p.ERR_BAD_MESSAGE))
                        continue
                    if me["match"]:
                        me["match"].dir[pid] = direction
                    else:
                        send(pid, p.ERROR, p.pack_error(p.ERR_INVALID_STATE))
                else:
                    send(pid, p.ERROR, p.pack_error(p.ERR_BAD_MESSAGE))
    except (ConnectionError, OSError, p.ProtocolError) as e:
        log(f"#{pid} conexión terminada: {e}")
    finally:
        with lock:
            match = players[pid]["match"]
            if match and not match.over:
                rival = match.right if pid == match.left else match.left
                match.finish(rival, p.END_FORFEIT)
            if pid in waiting:
                waiting.remove(pid)
            del players[pid]
        sock.close()
        log(f"#{pid} desconectado")


def main():
    global next_id
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", port))
    server.listen()
    log(f"Servidor de práctica escuchando en el puerto {port}")
    while True:
        sock, addr = server.accept()
        with lock:
            pid = next_id
            next_id += 1
            players[pid] = {"sock": sock, "nick": None, "match": None}
        log(f"#{pid} conectado desde {addr[0]}:{addr[1]}")
        threading.Thread(target=handle, args=(sock, pid), daemon=True).start()


if __name__ == "__main__":
    main()
