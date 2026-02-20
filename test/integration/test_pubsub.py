import redis
import time

def test_publish_no_subscribers(redis_client):
    """Publish to a channel with no subscribers should return 0."""
    result = redis_client.publish("mychannel", "hello")
    assert result == 0

import socket

def send_raw(sock, cmd_str):
    sock.sendall(cmd_str.encode("utf-8"))
    data = sock.recv(4096)
    return data.decode("utf-8")

def test_subscribe_blocks_commands(redis_server):
    """Once subscribed, client cannot issue normal commands."""
    host, port = redis_server["host"], redis_server["port"]
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.settimeout(2.0)
    
    # 1. Subscribe
    resp = send_raw(sock, "*2\r\n$9\r\nSUBSCRIBE\r\n$9\r\nmychannel\r\n")
    assert "subscribe" in resp
    assert "mychannel" in resp
    
    # 2. Try to send a normal command
    resp = send_raw(sock, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n")
    assert "ERR only (P)SUBSCRIBE / (P)UNSUBSCRIBE / PING / QUIT allowed in this context" in resp
    
    sock.close()

def test_pubsub_basic_flow(redis_server):
    """Test subscribing and receiving a published message."""
    host, port = redis_server["host"], redis_server["port"]
    
    # Client 1: Subscriber
    sub_client = redis.Redis(host=host, port=port, decode_responses=True)
    pubsub = sub_client.pubsub()
    pubsub.subscribe("news")
    
    # Drain subscribe confirmation
    msg = pubsub.get_message(timeout=1.0)
    assert msg["type"] == "subscribe"
    
    # Client 2: Publisher
    pub_client = redis.Redis(host=host, port=port, decode_responses=True)
    receivers = pub_client.publish("news", "Breaking News!")
    assert receivers == 1
    
    # Client 1: Receive message
    msg = pubsub.get_message(timeout=1.0)
    assert msg is not None
    assert msg["type"] == "message"
    assert msg["channel"] == "news"
    assert msg["data"] == "Breaking News!"
    
    # Unsubscribe
    pubsub.unsubscribe("news")
    msg = pubsub.get_message(timeout=1.0)
    assert msg["type"] == "unsubscribe"
    assert msg["channel"] == "news"
    assert msg["data"] == 0
    
    sub_client.close()
    pub_client.close()

def test_multiple_subscribers(redis_server):
    """Test multiple clients receiving the same broadcast."""
    host, port = redis_server["host"], redis_server["port"]
    
    sub1 = redis.Redis(host=host, port=port, decode_responses=True).pubsub()
    sub2 = redis.Redis(host=host, port=port, decode_responses=True).pubsub()
    
    sub1.subscribe("announcements")
    sub2.subscribe("announcements")
    sub1.get_message(timeout=1.0) # Drain sub msg
    sub2.get_message(timeout=1.0) # Drain sub msg
    
    pub_client = redis.Redis(host=host, port=port, decode_responses=True)
    receivers = pub_client.publish("announcements", "System update")
    assert receivers == 2
    
    msg1 = sub1.get_message(timeout=1.0)
    msg2 = sub2.get_message(timeout=1.0)
    
    assert msg1["data"] == "System update"
    assert msg2["data"] == "System update"
    
    sub1.close()
    sub2.close()
    pub_client.close()
