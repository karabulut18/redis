"""
Persistence restart test.
Writes keys, kills the server, restarts it with the same AOF file,
and checks that the data survives.
"""

import os
import subprocess
import socket
import time

import pytest
import redis


def wait_for_port(host, port, timeout=10):
    start = time.time()
    while time.time() - start < timeout:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(1)
                s.connect((host, port))
                return True
        except (ConnectionRefusedError, socket.timeout):
            time.sleep(0.1)
    return False


def test_persistence_survive_restart(redis_server, tmp_path_factory):
    """
    Write a set of keys, restart the server using the same AOF, and verify
    that all written keys are present after restart.
    """
    host = redis_server["host"]
    port = redis_server["port"]
    proc = redis_server["proc"]
    tmp_dir = redis_server["tmp_dir"]

    # 1. Write keys via the running server
    client = redis.Redis(host=host, port=port, decode_responses=True, socket_timeout=5)
    client.flushall()
    client.set("persist_str", "hello")
    client.set("persist_int", "42")
    client.lpush("persist_list", "a", "b", "c")
    client.sadd("persist_set", "x", "y")
    client.hset("persist_hash", mapping={"field1": "val1", "field2": "val2"})
    client.zadd("persist_zset", {"alpha": 1.0, "beta": 2.0})
    client.close()

    # 2. Gracefully stop the server (flush AOF)
    time.sleep(2)
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

    # 3. Restart the server with the SAME config/AOF
    cfg_path = os.path.join(tmp_dir, "test.conf")
    server_bin = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
        "build", "server", "redis_server"
    )
    log_file = open(os.path.join(tmp_dir, "server2.log"), "w")
    new_proc = subprocess.Popen(
        [server_bin, "--config", cfg_path],
        stdout=log_file,
        stderr=subprocess.STDOUT,
        cwd=tmp_dir,
    )

    if not wait_for_port(host, port):
        new_proc.kill()
        pytest.fail("Server failed to restart")

    # 4. Verify keys survived
    client2 = redis.Redis(host=host, port=port, decode_responses=True, socket_timeout=5)
    assert client2.get("persist_str") == "hello"
    assert client2.get("persist_int") == "42"
    assert set(client2.lrange("persist_list", 0, -1)) == {"a", "b", "c"}
    assert client2.smembers("persist_set") == {"x", "y"}
    assert client2.hgetall("persist_hash") == {"field1": "val1", "field2": "val2"}
    members = client2.zrange("persist_zset", 0, -1)
    assert set(members) == {"alpha", "beta"}
    client2.close()

    # 5. Update the fixture's proc reference so the session teardown kills the right process
    redis_server["proc"] = new_proc
    log_file.close()
