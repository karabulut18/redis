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

def test_rdb_serialization_and_deserialization(tmp_path):
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    server_bin = os.path.join(project_root, "build", "server", "redis_server")
    
    port = find_free_port()
    aof_path = str(tmp_path / "test.aof")
    cfg_path = str(tmp_path / "test.conf")
    with open(cfg_path, "w") as f:
        f.write(f"port {port}\n")
        f.write(f'appendfilename "{aof_path}"\n')
        f.write("appendfsync no\n")
        
    # 1. Start Server
    proc = subprocess.Popen([server_bin, "--config", cfg_path], cwd=str(tmp_path))
    assert wait_for_port(port)
    
    client = redis.Redis(host='127.0.0.1', port=port, decode_responses=True)
    
    # 2. Populate Data (all 5 types + TTL)
    client.set("mystr", "hello world", ex=100)
    
    client.rpush("mylist", "item1", "item2")
    client.sadd("myset", "member1", "member2")
    
    client.hset("myhash", "field1", "val1")
    client.hset("myhash", "field2", "val2")
    
    client.zadd("myzset", {"z1": 1.5, "z2": 2.5})
    
    # 3. SAVE synchronously
    resp = client.save()
    assert resp is True
    
    # 4. Stop Server
    client.close()
    proc.terminate()
    proc.wait(timeout=3)
    
    # 5. Delete AOF file so we strictly test RDB loading
    if os.path.exists(aof_path):
        os.remove(aof_path)
        
    assert os.path.exists(str(tmp_path / "dump.rdb")), "RDB file was not created"
    
    # 6. Restart Server
    proc = subprocess.Popen([server_bin, "--config", cfg_path], cwd=str(tmp_path))
    assert wait_for_port(port)
    
    client = redis.Redis(host='127.0.0.1', port=port, decode_responses=True)
    
    # 7. Verify Data
    assert client.get("mystr") == "hello world"
    ttl = client.ttl("mystr")
    assert 0 < ttl <= 100
    
    assert client.lrange("mylist", 0, -1) == ["item1", "item2"]
    
    smembers = client.smembers("myset")
    assert "member1" in smembers and "member2" in smembers
    
    assert client.hgetall("myhash") == {"field1": "val1", "field2": "val2"}
    
    assert client.zrange("myzset", 0, -1, withscores=True) == [("z1", 1.5), ("z2", 2.5)]
    
    # Cleanup
    client.close()
    proc.terminate()
    proc.wait(timeout=3)
    
def test_bgsave_creates_file(tmp_path):
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    server_bin = os.path.join(project_root, "build", "server", "redis_server")
    
    port = find_free_port()
    aof_path = str(tmp_path / "test.aof")
    cfg_path = str(tmp_path / "test.conf")
    with open(cfg_path, "w") as f:
        f.write(f"port {port}\n")
        f.write(f'appendfilename "{aof_path}"\n')
        f.write("appendfsync no\n")
        
    log_path = str(tmp_path / "server.log")
    log_file = open(log_path, "w")
        
    proc = subprocess.Popen([server_bin, "--config", cfg_path], cwd=str(tmp_path), stdout=log_file, stderr=subprocess.STDOUT)
    assert wait_for_port(port)
    
    client = redis.Redis(host='127.0.0.1', port=port, decode_responses=True)
    client.set("foo", "bar")
    
    # BGSAVE via raw socket to bypass redis-py magic
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', port))
    s.sendall(b"*1\r\n$6\r\nBGSAVE\r\n")
    resp = s.recv(1024)
    print(f"RAW RESP: {resp}")
    s.close()
    
    # Wait for the background process to finish writing dump.rdb
    rdb_path = str(tmp_path / "dump.rdb")
    for _ in range(20):
        if os.path.exists(rdb_path) and os.path.getsize(rdb_path) > 0:
            break
        time.sleep(0.1)
        
    if not os.path.exists(rdb_path):
        log_file.close()
        with open(log_path, 'r') as f:
            print("SERVER LOGS:\n" + f.read())
        print(f"TMP DIR CONTENTS: {os.listdir(str(tmp_path))}")
            
    assert os.path.exists(rdb_path)
    
    client.close()
    proc.terminate()
    proc.wait(timeout=3)
    if not log_file.closed:
        log_file.close()
