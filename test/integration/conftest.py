
import pytest
import subprocess
import time
import os
import socket
import redis

def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        return s.getsockname()[1]

def wait_for_port(port, timeout=5):
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

@pytest.fixture(scope="session")
def redis_server():
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    server_bin = os.path.join(project_root, "build", "server", "redis_server")
    
    if not os.path.exists(server_bin):
        pytest.fail(f"Server binary not found at {server_bin}. Did you build the project?")

    port = 6379 
    
    # Check if port 6379 is already in use
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        if s.connect_ex(('127.0.0.1', port)) == 0:
             print(f"Port {port} is already in use. Using existing server.")
             yield {"host": "127.0.0.1", "port": port}
             return

    print(f"Starting server at {server_bin}")
    # Use unbuffered output or pipe to file for debugging
    log_file = open("server_debug.log", "w")
    proc = subprocess.Popen([server_bin], stdout=log_file, stderr=subprocess.STDOUT)
    
    if not wait_for_port(port):
        proc.kill()
        pytest.fail("Server failed to start or bind port 6379")

    print("Server started and listening")
    yield {"host": "127.0.0.1", "port": port}

    print("Stopping server")
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
    log_file.close()

@pytest.fixture(scope="function")
def redis_client(redis_server):
    # Set socket_timeout to avoid hanging indefinitely
    client = redis.Redis(host=redis_server["host"], port=redis_server["port"], decode_responses=True, socket_timeout=5)
    if not client.ping():
        pytest.fail("Failed to ping server")
    yield client
    try:
        client.flushall()
    except redis.ConnectionError:
        pass 
    client.close()
