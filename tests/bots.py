"""
Bots de prueba para PongServer.

Cada bot es un jugador falso que usa el mismo protocolo que el cliente
real (client/protocol.py): se registra, pide partida, mueve su paleta
siguiendo la bola y juega hasta que llega GAME_OVER.

Uso:
    python3 tests/bots.py <IP> <PUERTO> [parejas]          partidas completas
    python3 tests/bots.py <IP> <PUERTO> [parejas] --quit   un jugador abandona

Ejemplos:
    python3 tests/bots.py localhost 8080 5
    python3 tests/bots.py localhost 8080 2 --quit
"""
import os
import random
import socket
import sys
import threading
import time

# Reutilizamos el protocolo del cliente para no escribirlo dos veces.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "client"))
import protocol as p  # noqa: E402

GAME_WIDTH = 800
PADDLE_HEIGHT = 100
MATCH_TIMEOUT = 180   # segundos máximos que puede durar una partida
PING_EVERY = 10       # el servidor cierra la conexión a los 30 s sin mensajes


class Bot:
    def __init__(self, number, host, port, quit_after_points=None):
        self.number = number
        self.nickname = f"bot{number}"
        self.sock = socket.create_connection((host, port), timeout=MATCH_TIMEOUT)
        self.quit_after_points = quit_after_points

        self.side = None
        self.opponent = None
        self.direction = p.MOVE_STOP
        self.last_ball_x = None
        self.will_defend = True   # decide si atrapa la bola que viene
        self.last_sent = time.monotonic()

        self.result = None        # GAME_OVER recibido
        self.error = None
        self.states = 0           # cuántos STATE recibió

    def send(self, msg_type, payload=b""):
        p.send_message(self.sock, msg_type, payload)
        self.last_sent = time.monotonic()

    def move(self, direction):
        # Solo se envía MOVE cuando cambia la dirección, igual que el cliente.
        if direction != self.direction:
            self.direction = direction
            self.send(p.MOVE, p.pack_move(direction))

    def ball_is_coming(self, ball_x):
        """True si la bola se mueve hacia mi lado de la cancha."""
        if self.last_ball_x is None:
            return False
        moving_left = ball_x < self.last_ball_x
        return moving_left if self.side == p.SIDE_LEFT else not moving_left

    def play(self, state):
        """Seguir la bola con la paleta, pero no siempre (para que haya puntos)."""
        coming = self.ball_is_coming(state.ball_x)

        # Cada vez que la bola empieza a venir, el bot decide si la defiende.
        if coming and not self.was_coming:
            self.will_defend = random.random() < 0.7
        self.was_coming = coming
        self.last_ball_x = state.ball_x

        my_y = state.paddle_left_y if self.side == p.SIDE_LEFT else state.paddle_right_y
        paddle_center = my_y + PADDLE_HEIGHT / 2

        if not coming or not self.will_defend:
            self.move(p.MOVE_STOP)
        elif state.ball_y < paddle_center - 10:
            self.move(p.MOVE_UP)
        elif state.ball_y > paddle_center + 10:
            self.move(p.MOVE_DOWN)
        else:
            self.move(p.MOVE_STOP)

    def run(self):
        self.was_coming = False
        try:
            self.send(p.REGISTER, p.pack_register(self.nickname, f"{self.nickname}@test.com"))

            while self.result is None:
                msg_type, payload = p.recv_message(self.sock)

                if msg_type == p.REGISTER_RESULT:
                    res = p.unpack_register_result(payload)
                    if res.result != p.REG_OK:
                        raise RuntimeError(f"registro rechazado ({res.result})")
                    self.send(p.JOIN)

                elif msg_type == p.MATCH_START:
                    start = p.unpack_match_start(payload)
                    self.side, self.opponent = start.side, start.opponent

                elif msg_type == p.STATE:
                    self.states += 1
                    state = p.unpack_state(payload)
                    points = state.score_left + state.score_right

                    # Prueba de abandono: este bot se va a mitad de partida.
                    if self.quit_after_points is not None and points >= self.quit_after_points:
                        self.send(p.QUIT)
                        self.result = "QUIT"
                        break
                    self.play(state)

                elif msg_type == p.GAME_OVER:
                    self.result = p.unpack_game_over(payload)

                elif msg_type == p.ERROR:
                    raise RuntimeError(f"ERROR del servidor, código {p.unpack_error(payload)}")

                # WAITING no necesita respuesta: solo hay que esperar.

                if time.monotonic() - self.last_sent >= PING_EVERY:
                    self.send(p.PING)

        except Exception as e:  # noqa: BLE001 - se reporta cualquier fallo
            self.error = f"{type(e).__name__}: {e}"
        finally:
            self.sock.close()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    quit_test = "--quit" in sys.argv

    if len(args) < 2:
        print(__doc__)
        sys.exit(1)

    host, port = args[0], int(args[1])
    pairs = int(args[2]) if len(args) > 2 else 3
    total = pairs * 2

    # En la prueba de abandono, los bots pares se van después del primer punto.
    bots = [Bot(i + 1, host, port, quit_after_points=1 if quit_test and i % 2 == 0 else None)
            for i in range(total)]

    print(f"Lanzando {total} bots ({pairs} partidas simultáneas)"
          f"{' con abandono' if quit_test else ''}...")
    start = time.monotonic()

    threads = [threading.Thread(target=b.run) for b in bots]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    elapsed = time.monotonic() - start
    ok = 0
    for b in bots:
        if b.error:
            print(f"  {b.nickname}: FALLÓ -> {b.error}")
        elif b.result == "QUIT":
            print(f"  {b.nickname}: abandonó a propósito (vs {b.opponent})")
            ok += 1
        else:
            r = b.result
            reason = "puntos" if r.reason == p.END_SCORE else "abandono"
            print(f"  {b.nickname}: vs {b.opponent:6} marcador {r.score_left}-{r.score_right}, "
                  f"ganador #{r.winner_id} por {reason}, {b.states} estados recibidos")
            ok += 1

    print(f"\n{ok}/{total} bots OK en {elapsed:.1f} s")
    sys.exit(0 if ok == total else 1)


if __name__ == "__main__":
    main()
