"""Pruebas del protocolo en Python.
Usan los mismos bytes que server/tests/test_protocol.c: si las dos pasan,
el cliente y el servidor hablan igual.

Uso: python3 -m unittest test_protocol   (desde la carpeta client/)
"""
import socket
import unittest

import protocol as p

REGISTER_ANA = bytes([3]) + b"Ana" + bytes([8]) + b"ana@x.co"
REGISTER_RESULT_OK_7 = bytes([0, 0, 0, 0, 7])
MATCH_START_3_RIGHT_BOB = bytes([0, 0, 0, 3, 1, 3]) + b"Bob"
STATE_SAMPLE = bytes([0x01, 0x90, 0x01, 0x2D, 0x00, 0xFA, 0x00, 0x00, 2, 4])
GAME_OVER_7_5_2 = bytes([0, 0, 0, 7, 5, 2, 0])


class SameBytesAsC(unittest.TestCase):
    def test_register(self):
        self.assertEqual(p.pack_register("Ana", "ana@x.co"), REGISTER_ANA)
        self.assertEqual(p.unpack_register(REGISTER_ANA), ("Ana", "ana@x.co"))

    def test_full_frame_has_header(self):
        frame = p.encode_message(p.REGISTER, REGISTER_ANA)
        self.assertEqual(frame[:4], bytes([1, 0x01, 0x00, 0x0D]))
        self.assertEqual(frame[4:], REGISTER_ANA)

    def test_move(self):
        self.assertEqual(p.pack_move(p.MOVE_STOP), b"\x00")
        self.assertEqual(p.pack_move(p.MOVE_UP), b"\x01")
        self.assertEqual(p.pack_move(p.MOVE_DOWN), b"\x02")
        self.assertEqual(p.unpack_move(b"\x02"), p.MOVE_DOWN)

    def test_register_result(self):
        self.assertEqual(p.pack_register_result(p.REG_OK, 7), REGISTER_RESULT_OK_7)
        self.assertEqual(p.unpack_register_result(REGISTER_RESULT_OK_7), (p.REG_OK, 7))
        # Si falla, el id viaja en 0 (igual que en C).
        self.assertEqual(p.pack_register_result(p.REG_TAKEN, 7), bytes([2, 0, 0, 0, 0]))

    def test_match_start(self):
        self.assertEqual(p.pack_match_start(3, p.SIDE_RIGHT, "Bob"), MATCH_START_3_RIGHT_BOB)
        self.assertEqual(p.unpack_match_start(MATCH_START_3_RIGHT_BOB),
                         (3, p.SIDE_RIGHT, "Bob"))

    def test_state(self):
        self.assertEqual(p.pack_state(400.4, 300.6, 250.0, -2.0, 2, 4), STATE_SAMPLE)
        self.assertEqual(p.unpack_state(STATE_SAMPLE), (400, 301, 250, 0, 2, 4))

    def test_game_over(self):
        self.assertEqual(p.pack_game_over(7, 5, 2, p.END_SCORE), GAME_OVER_7_5_2)
        self.assertEqual(p.unpack_game_over(GAME_OVER_7_5_2), (7, 5, 2, p.END_SCORE))

    def test_error(self):
        self.assertEqual(p.pack_error(p.ERR_INVALID_STATE), b"\x03")
        self.assertEqual(p.unpack_error(b"\x03"), p.ERR_INVALID_STATE)


class RejectsBadMessages(unittest.TestCase):
    def test_bad_register(self):
        for bad in (b"", REGISTER_ANA[:-1], REGISTER_ANA + b"x",
                    bytes([0, 1]) + b"a", bytes([2]) + b"A\0" + bytes([1]) + b"a"):
            with self.assertRaises(p.ProtocolError):
                p.unpack_register(bad)

    def test_text_limits(self):
        with self.assertRaises(p.ProtocolError):
            p.pack_register("a" * 33, "a@b")
        with self.assertRaises(p.ProtocolError):
            p.pack_register("", "a@b")

    def test_bad_fixed_sizes(self):
        with self.assertRaises(p.ProtocolError):
            p.unpack_state(STATE_SAMPLE[:-1])
        with self.assertRaises(p.ProtocolError):
            p.unpack_move(b"\x03")
        with self.assertRaises(p.ProtocolError):
            p.unpack_match_start(bytes([0, 0, 0, 3, 5, 1]) + b"B")


class OverASocket(unittest.TestCase):
    def test_message_split_in_pieces_is_rebuilt(self):
        a, b = socket.socketpair()
        with a, b:
            frame = p.encode_message(p.STATE, STATE_SAMPLE)
            # Enviar byte por byte, como podría pasar en TCP.
            for i in range(len(frame)):
                a.sendall(frame[i:i + 1])
            self.assertEqual(p.recv_message(b), (p.STATE, STATE_SAMPLE))

    def test_bad_version_is_rejected(self):
        a, b = socket.socketpair()
        with a, b:
            a.sendall(bytes([2, p.PING, 0, 0]))
            with self.assertRaises(p.ProtocolError):
                p.recv_message(b)


if __name__ == "__main__":
    unittest.main()
