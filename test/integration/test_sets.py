
import pytest

def test_sadd_smembers(redis_client):
    assert redis_client.sadd("myset", "Hello") == 1
    assert redis_client.sadd("myset", "World") == 1
    assert redis_client.sadd("myset", "World") == 0
    members = redis_client.smembers("myset")
    assert "Hello" in members
    assert "World" in members
    assert len(members) == 2

def test_srem(redis_client):
    redis_client.sadd("myset2", "one", "two")
    assert redis_client.srem("myset2", "one") == 1
    assert redis_client.srem("myset2", "nonexistent") == 0
    members = redis_client.smembers("myset2")
    assert members == {"two"}

def test_sismember(redis_client):
    redis_client.sadd("myset3", "one")
    assert redis_client.sismember("myset3", "one") == 1
    assert redis_client.sismember("myset3", "two") == 0

def test_scard(redis_client):
    redis_client.sadd("myset4", "a", "b", "c")
    assert redis_client.scard("myset4") == 3
