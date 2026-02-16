
import pytest
import time

def test_lpush_lrange(redis_client):
    assert redis_client.lpush("mylist", "world") == 1
    assert redis_client.lpush("mylist", "hello") == 2
    assert redis_client.lrange("mylist", 0, -1) == ["hello", "world"]

def test_rpush_lrange(redis_client):
    assert redis_client.rpush("mylist2", "hello") == 1
    assert redis_client.rpush("mylist2", "world") == 2
    assert redis_client.lrange("mylist2", 0, -1) == ["hello", "world"]

def test_lpop(redis_client):
    redis_client.rpush("mylist3", "one")
    redis_client.rpush("mylist3", "two")
    redis_client.rpush("mylist3", "three")
    assert redis_client.lpop("mylist3") == "one"
    assert redis_client.lrange("mylist3", 0, -1) == ["two", "three"]

def test_rpop(redis_client):
    redis_client.rpush("mylist4", "one")
    redis_client.rpush("mylist4", "two")
    redis_client.rpush("mylist4", "three")
    assert redis_client.rpop("mylist4") == "three"
    assert redis_client.lrange("mylist4", 0, -1) == ["one", "two"]

def test_llen(redis_client):
    redis_client.lpush("mylist5", "a", "b", "c")
    assert redis_client.llen("mylist5") == 3
    
def test_lrange_subrange(redis_client):
    redis_client.rpush("mylist6", "one", "two", "three")
    assert redis_client.lrange("mylist6", 0, 0) == ["one"]
    assert redis_client.lrange("mylist6", -100, 100) == ["one", "two", "three"]
    assert redis_client.lrange("mylist6", 5, 10) == []
