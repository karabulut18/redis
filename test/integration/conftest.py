
import pytest
import subprocess
import tempfile
import time
import os
import socket
import redis


def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        return s.getsockname()[1]


def wait_for_port(port, timeout=10):
    start = time.time()
    while time.time() - start < timeout:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(1)
                s.connect(('127.0.0.1', port))
                return True
        except (ConnectionRefusedError, socket.timeout):
            time.sleep(0.1)
    return False


@pytest.fixture(scope="function")
def redis_server(tmp_path_factory):
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    server_bin = os.path.join(project_root, "build", "server", "redis_server")

    if not os.path.exists(server_bin):
        pytest.fail(f"Server binary not found at {server_bin}. Run: cd build && make -j4")

    port = find_free_port()

    # Write a temp config so the server uses our chosen port and a temp AOF file
    tmp_dir = tmp_path_factory.mktemp("redis_server")
    aof_path = str(tmp_dir / "test.aof")
    cfg_path = str(tmp_dir / "test.conf")
    with open(cfg_path, "w") as f:
        f.write(f"port {port}\n")
        f.write(f'appendfilename "{aof_path}"\n')
        f.write("appendfsync no\n")  # fastest for tests

    log_path = str(tmp_dir / "server.log")
    log_file = open(log_path, "w")

    proc = subprocess.Popen(
        [server_bin, "--config", cfg_path],
        stdout=log_file,
        stderr=subprocess.STDOUT,
        cwd=str(tmp_dir),  # run in tmp dir so relative AOF paths work
    )

    if not wait_for_port(port):
        proc.kill()
        log_file.close()
        with open(log_path) as lf:
            print(lf.read())
        pytest.fail(f"Server failed to start on port {port}")

    print(f"Server started on port {port} (pid={proc.pid})")
    yield {"host": "127.0.0.1", "port": port, "proc": proc, "tmp_dir": str(tmp_dir)}

    print("Stopping server")
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
    log_file.close()


@pytest.fixture(scope="function")
def redis_client(redis_server):
    host, port = redis_server["host"], redis_server["port"]
    client = redis.Redis(host=host, port=port, decode_responses=True, socket_timeout=5)
    if not client.ping():
        pytest.fail("Failed to ping server")
    yield client
    try:
        client.flushall()
    except redis.ConnectionError:
        pass
    client.close()


@pytest.fixture(scope="function")
def raw_client(redis_server):
    """Returns a raw socket connected to the server for low-level RESP testing."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((redis_server["host"], redis_server["port"]))
    yield s
    s.close()
