"""MyAppGameProtocol en Python: el mismo diccionario de server/src/protocol.h.

Cada mensaje = encabezado de 4 bytes + payload:
    | version 1B | tipo 1B | longitud 2B | payload (máx. 512 B) |
Todos los números van en orden de red (big-endian), por eso los formatos
de struct empiezan con "!".
"""
import struct
from collections import namedtuple

VERSION = 1
HEADER = struct.Struct("!BBH")
HEADER_SIZE = HEADER.size
MAX_PAYLOAD = 512

NICKNAME_MAX = 32
EMAIL_MAX = 128

# Tipos de mensaje. Cliente -> servidor: 0x01..0x7F. Servidor -> cliente: 0x81..0xFF.
REGISTER = 0x01
JOIN = 0x02
MOVE = 0x03
PING = 0x04
QUIT = 0x05

REGISTER_RESULT = 0x81
WAITING = 0x82
MATCH_START = 0x83
STATE = 0x84
GAME_OVER = 0x85
ERROR = 0x86

TYPE_NAMES = {
    REGISTER: "REGISTER", JOIN: "JOIN", MOVE: "MOVE", PING: "PING",
    QUIT: "QUIT", REGISTER_RESULT: "REGISTER_RESULT", WAITING: "WAITING",
    MATCH_START: "MATCH_START", STATE: "STATE", GAME_OVER: "GAME_OVER",
    ERROR: "ERROR",
}

# Dirección en MOVE
MOVE_STOP, MOVE_UP, MOVE_DOWN = 0, 1, 2

# Resultado en REGISTER_RESULT
REG_OK, REG_INVALID, REG_TAKEN, REG_FULL = 0, 1, 2, 3

# Lado en MATCH_START
SIDE_LEFT, SIDE_RIGHT = 0, 1

# Motivo en GAME_OVER
END_SCORE, END_FORFEIT = 0, 1

# Código en ERROR
ERR_BAD_MESSAGE, ERR_NOT_REGISTERED, ERR_INVALID_STATE, ERR_SERVER_FULL = 1, 2, 3, 4

_REGISTER_RESULT = struct.Struct("!BI")
_MATCH_START_HEAD = struct.Struct("!IB")
_STATE = struct.Struct("!HHHHBB")
_GAME_OVER = struct.Struct("!IBBB")

RegisterResult = namedtuple("RegisterResult", "result player_id")
MatchStart = namedtuple("MatchStart", "match_id side opponent")
GameState = namedtuple(
    "GameState", "ball_x ball_y paddle_left_y paddle_right_y score_left score_right"
)
GameOver = namedtuple("GameOver", "winner_id score_left score_right reason")


class ProtocolError(Exception):
    """Mensaje mal formado o que no cumple la especificación."""


# ---------- Encabezado y envío/recepción ----------

def encode_message(msg_type, payload=b""):
    if len(payload) > MAX_PAYLOAD:
        raise ProtocolError("payload demasiado grande")
    return HEADER.pack(VERSION, msg_type, len(payload)) + payload


def send_message(sock, msg_type, payload=b""):
    sock.sendall(encode_message(msg_type, payload))


def recv_all(sock, n):
    """TCP puede entregar medio mensaje: repetimos recv hasta tener n bytes."""
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("la otra parte cerró la conexión")
        data += chunk
    return data


def recv_message(sock):
    """Devuelve (tipo, payload). Lanza ConnectionError si se cerró."""
    version, msg_type, length = HEADER.unpack(recv_all(sock, HEADER_SIZE))
    if version != VERSION or length > MAX_PAYLOAD:
        raise ProtocolError(f"encabezado inválido (versión {version}, largo {length})")
    payload = recv_all(sock, length) if length else b""
    return msg_type, payload


# ---------- Textos con largo de 1 byte ----------

def _pack_text(text, max_len):
    data = text.encode("utf-8")
    if not 1 <= len(data) <= max_len:
        raise ProtocolError(f"texto de {len(data)} bytes (permitido 1..{max_len})")
    return bytes([len(data)]) + data


def _unpack_text(payload, offset, max_len):
    """Lee [largo 1B][texto] desde offset. Devuelve (texto, siguiente offset)."""
    if offset >= len(payload):
        raise ProtocolError("falta el largo del texto")
    size = payload[offset]
    end = offset + 1 + size
    if size == 0 or size > max_len or end > len(payload):
        raise ProtocolError("largo de texto inválido")
    raw = payload[offset + 1:end]
    if b"\0" in raw:
        raise ProtocolError("texto con byte nulo")
    return raw.decode("utf-8", errors="replace"), end


# ---------- Cliente -> servidor ----------

def pack_register(nickname, email):
    return _pack_text(nickname, NICKNAME_MAX) + _pack_text(email, EMAIL_MAX)


def unpack_register(payload):
    nickname, offset = _unpack_text(payload, 0, NICKNAME_MAX)
    email, offset = _unpack_text(payload, offset, EMAIL_MAX)
    if offset != len(payload):
        raise ProtocolError("bytes de más en REGISTER")
    return nickname, email


def pack_move(direction):
    if direction not in (MOVE_STOP, MOVE_UP, MOVE_DOWN):
        raise ProtocolError("dirección inválida")
    return bytes([direction])


def unpack_move(payload):
    if len(payload) != 1 or payload[0] not in (MOVE_STOP, MOVE_UP, MOVE_DOWN):
        raise ProtocolError("MOVE inválido")
    return payload[0]


# ---------- Servidor -> cliente ----------

def _unpack_fixed(fmt, payload, name):
    if len(payload) != fmt.size:
        raise ProtocolError(f"{name} debe medir {fmt.size} bytes")
    return fmt.unpack(payload)


def pack_register_result(result, player_id):
    return _REGISTER_RESULT.pack(result, player_id if result == REG_OK else 0)


def unpack_register_result(payload):
    return RegisterResult(*_unpack_fixed(_REGISTER_RESULT, payload, "REGISTER_RESULT"))


def pack_match_start(match_id, side, opponent):
    return _MATCH_START_HEAD.pack(match_id, side) + _pack_text(opponent, NICKNAME_MAX)


def unpack_match_start(payload):
    if len(payload) < _MATCH_START_HEAD.size:
        raise ProtocolError("MATCH_START muy corto")
    match_id, side = _MATCH_START_HEAD.unpack_from(payload)
    if side not in (SIDE_LEFT, SIDE_RIGHT):
        raise ProtocolError("lado inválido")
    opponent, offset = _unpack_text(payload, _MATCH_START_HEAD.size, NICKNAME_MAX)
    if offset != len(payload):
        raise ProtocolError("bytes de más en MATCH_START")
    return MatchStart(match_id, side, opponent)


def pack_state(ball_x, ball_y, paddle_left_y, paddle_right_y, score_left, score_right):
    def px(v):
        # Mismo redondeo que to_pixels() en C (round() de Python redondea 300.5 a 300).
        return 0 if v <= 0 else min(65535, int(v + 0.5))
    return _STATE.pack(px(ball_x), px(ball_y), px(paddle_left_y), px(paddle_right_y),
                       min(score_left, 255), min(score_right, 255))


def unpack_state(payload):
    return GameState(*_unpack_fixed(_STATE, payload, "STATE"))


def pack_game_over(winner_id, score_left, score_right, reason):
    return _GAME_OVER.pack(winner_id, score_left, score_right, reason)


def unpack_game_over(payload):
    return GameOver(*_unpack_fixed(_GAME_OVER, payload, "GAME_OVER"))


def pack_error(code):
    return bytes([code])


def unpack_error(payload):
    if len(payload) != 1:
        raise ProtocolError("ERROR debe medir 1 byte")
    return payload[0]
