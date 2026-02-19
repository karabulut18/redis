"""
Refactored from the old standalone test_legacy.py script into proper pytest functions.
Tests raw RESP wire protocol via a direct socket connection.
"""


def send_raw(sock, command: str) -> str:
    sock.sendall(command.encode())
    return sock.recv(4096).decode()


def test_ping_raw(raw_client):
    resp = send_raw(raw_client, "*1\r\n$4\r\nPING\r\n")
    assert "+PONG" in resp


def test_set_get_raw(raw_client):
    resp = send_raw(raw_client, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n")
    assert "+OK" in resp

    resp = send_raw(raw_client, "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n")
    assert "$3\r\nbar" in resp


def test_unknown_command_raw(raw_client):
    resp = send_raw(raw_client, "*1\r\n$7\r\nNOTREAL\r\n")
    assert resp.startswith("-")  # error response
