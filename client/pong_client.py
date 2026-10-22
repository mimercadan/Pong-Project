"""PongClient: cliente gráfico (pygame) que habla MyAppGameProtocol.

Uso: python3 pong_client.py <IP> <PUERTO> <nickname> <email>

Teclas: flecha arriba / W y flecha abajo / S para mover la paleta.
        ESPACIO para jugar otra vez al terminar; ESC para salir.

El cliente no calcula la física: solo envía las teclas (MOVE) y dibuja el
estado (STATE) que le manda el servidor.
"""
import queue
import socket
import sys
import threading
import time

import pygame

import protocol as p

WIDTH, HEIGHT = 800, 600              # misma cancha que server/src/game.h
PADDLE_W, PADDLE_H, MARGIN = 20, 100, 30
BALL_R = 10
PING_EVERY = 10                       # el servidor cierra a los 30 s sin mensajes
FPS = 60

WHITE = (240, 240, 240)
GRAY = (110, 110, 110)
ACCENT = (90, 200, 250)
BLACK = (15, 15, 20)

REG_TEXT = {
    p.REG_INVALID: "Registro rechazado: nickname sin espacios y email con una @",
    p.REG_TAKEN: "Esta conexión ya estaba registrada",
    p.REG_FULL: "El servidor está lleno, intenta más tarde",
}
ERR_TEXT = {
    p.ERR_BAD_MESSAGE: "El servidor no entendió el mensaje",
    p.ERR_NOT_REGISTERED: "Primero hay que registrarse",
    p.ERR_INVALID_STATE: "Esa acción no aplica en este momento",
    p.ERR_SERVER_FULL: "No hay cupo para más partidas",
}


def log(msg):
    print(time.strftime("[%H:%M:%S]"), msg, flush=True)


class Connection:
    """Socket + hilo lector. Lo recibido llega a la cola `inbox`."""

    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(None)
        self.inbox = queue.Queue()
        self.last_sent = time.monotonic()
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        try:
            while True:
                msg_type, payload = p.recv_message(self.sock)
                if msg_type != p.STATE:
                    log(f"RECV <- {p.TYPE_NAMES.get(msg_type, hex(msg_type))} ({len(payload)} B)")
                self.inbox.put((msg_type, payload))
        except (OSError, ConnectionError, p.ProtocolError) as e:
            self.inbox.put((None, str(e)))

    def send(self, msg_type, payload=b""):
        p.send_message(self.sock, msg_type, payload)
        self.last_sent = time.monotonic()
        if msg_type != p.PING:
            log(f"SEND -> {p.TYPE_NAMES[msg_type]} ({len(payload)} B)")

    def keep_alive(self):
        if time.monotonic() - self.last_sent >= PING_EVERY:
            p.send_message(self.sock, p.PING)
            self.last_sent = time.monotonic()

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


class Game:
    def __init__(self, conn, nickname):
        self.conn = conn
        self.nickname = nickname
        self.screen_name = "registering"   # registering, waiting, playing, over, closed
        self.notice = ""
        self.player_id = 0
        self.side = p.SIDE_LEFT
        self.opponent = ""
        self.state = None
        self.result = None
        self.direction = p.MOVE_STOP

    # ---------- Mensajes del servidor ----------

    def handle(self, msg_type, payload):
        if msg_type is None:
            self.screen_name = "closed"
            self.notice = f"Conexión cerrada: {payload}"
            return
        try:
            if msg_type == p.REGISTER_RESULT:
                res = p.unpack_register_result(payload)
                if res.result == p.REG_OK:
                    self.player_id = res.player_id
                    self.conn.send(p.JOIN)
                else:
                    self.screen_name = "closed"
                    self.notice = REG_TEXT.get(res.result, "Registro rechazado")
            elif msg_type == p.WAITING:
                self.screen_name = "waiting"
            elif msg_type == p.MATCH_START:
                start = p.unpack_match_start(payload)
                self.side, self.opponent = start.side, start.opponent
                self.state, self.result = None, None
                self.direction = p.MOVE_STOP
                self.screen_name = "playing"
                self.notice = ""
            elif msg_type == p.STATE:
                self.state = p.unpack_state(payload)
            elif msg_type == p.GAME_OVER:
                self.result = p.unpack_game_over(payload)
                self.screen_name = "over"
            elif msg_type == p.ERROR:
                self.notice = ERR_TEXT.get(p.unpack_error(payload), "Error del servidor")
        except p.ProtocolError as e:
            log(f"Mensaje inválido del servidor: {e}")

    # ---------- Teclado ----------

    def update_direction(self, keys):
        up = keys[pygame.K_UP] or keys[pygame.K_w]
        down = keys[pygame.K_DOWN] or keys[pygame.K_s]
        direction = p.MOVE_UP if up and not down else p.MOVE_DOWN if down and not up else p.MOVE_STOP
        if direction != self.direction:      # solo se envía cuando cambia
            self.direction = direction
            self.conn.send(p.MOVE, p.pack_move(direction))

    def play_again(self):
        self.screen_name = "waiting"
        self.conn.send(p.JOIN)

    # ---------- Dibujo ----------

    def draw(self, screen, font, big):
        screen.fill(BLACK)
        if self.screen_name in ("playing", "over") and self.state:
            self.draw_field(screen, font, big)
        if self.screen_name == "registering":
            center_text(screen, big, "Conectando...", HEIGHT // 2)
        elif self.screen_name == "waiting":
            center_text(screen, big, "Esperando rival...", HEIGHT // 2)
            center_text(screen, font, f"Jugador: {self.nickname}", HEIGHT // 2 + 50, GRAY)
        elif self.screen_name == "over" and self.result:
            won = self.result.winner_id == self.player_id
            title = "¡Ganaste!" if won else "Perdiste"
            if self.result.reason == p.END_FORFEIT:
                title += " (el rival se fue)" if won else " (abandono)"
            center_text(screen, big, title, HEIGHT // 2 - 30, ACCENT)
            center_text(screen, font, f"{self.result.score_left} - {self.result.score_right}",
                        HEIGHT // 2 + 20)
            center_text(screen, font, "ESPACIO: jugar otra vez    ESC: salir",
                        HEIGHT // 2 + 60, GRAY)
        elif self.screen_name == "closed":
            center_text(screen, font, self.notice, HEIGHT // 2, ACCENT)
            center_text(screen, font, "ESC: salir", HEIGHT // 2 + 40, GRAY)
        if self.notice and self.screen_name != "closed":
            center_text(screen, font, self.notice, HEIGHT - 30, ACCENT)

    def draw_field(self, screen, font, big):
        s = self.state
        for y in range(0, HEIGHT, 30):
            pygame.draw.rect(screen, GRAY, (WIDTH // 2 - 2, y, 4, 15))
        left_color = ACCENT if self.side == p.SIDE_LEFT else WHITE
        right_color = ACCENT if self.side == p.SIDE_RIGHT else WHITE
        pygame.draw.rect(screen, left_color, (MARGIN, s.paddle_left_y, PADDLE_W, PADDLE_H))
        pygame.draw.rect(screen, right_color,
                         (WIDTH - MARGIN - PADDLE_W, s.paddle_right_y, PADDLE_W, PADDLE_H))
        pygame.draw.circle(screen, WHITE, (s.ball_x, s.ball_y), BALL_R)
        center_text(screen, big, str(s.score_left), 40, WHITE, WIDTH // 4)
        center_text(screen, big, str(s.score_right), 40, WHITE, 3 * WIDTH // 4)
        me, rival = f"{self.nickname} (tú)", self.opponent
        left, right = (me, rival) if self.side == p.SIDE_LEFT else (rival, me)
        center_text(screen, font, left, 80, left_color, WIDTH // 4)
        center_text(screen, font, right, 80, right_color, 3 * WIDTH // 4)


def center_text(screen, font, text, y, color=WHITE, x=WIDTH // 2):
    img = font.render(text, True, color)
    screen.blit(img, img.get_rect(center=(x, y)))


def main():
    if len(sys.argv) != 5:
        print("Uso: python3 pong_client.py <IP> <PUERTO> <nickname> <email>")
        sys.exit(1)
    host, port, nickname, email = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]

    try:
        register = p.pack_register(nickname, email)
    except p.ProtocolError as e:
        print(f"Datos inválidos: {e}")
        sys.exit(1)

    try:
        conn = Connection(host, port)
    except OSError as e:
        print(f"No se pudo conectar a {host}:{port}: {e}")
        sys.exit(1)
    log(f"Conectado a {host}:{port}")

    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption(f"Pong - {nickname}")
    font = pygame.font.SysFont(None, 32)
    big = pygame.font.SysFont(None, 64)
    clock = pygame.time.Clock()

    game = Game(conn, nickname)
    running = True
    try:
        conn.send(p.REGISTER, register)
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                    elif event.key == pygame.K_SPACE and game.screen_name == "over":
                        game.play_again()

            while not conn.inbox.empty():
                game.handle(*conn.inbox.get())

            if game.screen_name == "playing":
                game.update_direction(pygame.key.get_pressed())
            if game.screen_name != "closed":
                conn.keep_alive()

            game.draw(screen, font, big)
            pygame.display.flip()
            clock.tick(FPS)

        if game.screen_name != "closed":
            conn.send(p.QUIT)
    except OSError as e:
        log(f"Error de red: {e}")
    finally:
        conn.close()
        pygame.quit()


if __name__ == "__main__":
    main()
