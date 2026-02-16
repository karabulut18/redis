
import pytest

def test_hset_hget(redis_client):
    assert redis_client.hset("myhash", "field1", "Hello") == 1
    assert redis_client.hget("myhash", "field1") == "Hello"
    assert redis_client.hset("myhash", "field1", "World") == 0 # update
    assert redis_client.hget("myhash", "field1") == "World"

def test_hdel(redis_client):
    redis_client.hset("myhash2", "field1", "foo")
    assert redis_client.hdel("myhash2", "field1") == 1
    assert redis_client.hdel("myhash2", "field1") == 0
    assert redis_client.hget("myhash2", "field1") is None

def test_hlen(redis_client):
    redis_client.hset("myhash3", "f1", "v1")
    redis_client.hset("myhash3", "f2", "v2")
    assert redis_client.hlen("myhash3") == 2

def test_hgetall(redis_client):
    redis_client.hset("myhash4", "f1", "v1")
    redis_client.hset("myhash4", "f2", "v2")
    # redis-py returns dict
    res = redis_client.hgetall("myhash4")
    assert res == {"f1": "v1", "f2": "v2"}
