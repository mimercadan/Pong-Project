"""Bots de prueba: abren varias conexiones a la vez y envían mensajes binarios.
Uso: python3 bots.py <IP> <PUERTO> [cantidad_bots]"""
import socket, struct, sys, threading, time

VERSION = 1
HEADER = "!BBH"   # version (1B), tipo (1B), longitud (2B) en orden de red

def recv_all(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("el servidor cerró la conexión")
        data += chunk
    return data

def bot(i, host, port, results):
    try:
        s = socket.create_connection((host, port), timeout=5)
        for n in range(5):
            payload = f"bot{i}-msg{n}".encode()
            s.sendall(struct.pack(HEADER, VERSION, 7, len(payload)) + payload)
            v, t, length = struct.unpack(HEADER, recv_all(s, 4))
            reply = recv_all(s, length)
            assert reply == payload, "respuesta distinta"
            time.sleep(0.05)
        s.close()
        results[i] = True
    except Exception as e:
        print(f"Bot {i} falló: {e}")
        results[i] = False

if __name__ == "__main__":
    host, port = sys.argv[1], int(sys.argv[2])
    total = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    results = [False] * total
    hilos = [threading.Thread(target=bot, args=(i, host, port, results)) for i in range(total)]
    for h in hilos: h.start()
    for h in hilos: h.join()
    print(f"{sum(results)}/{total} bots OK")
