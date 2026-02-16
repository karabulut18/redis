
import pytest

def test_zadd_zscore(redis_client):
    assert redis_client.zadd("myzset", {"one": 1}) == 1
    assert redis_client.zadd("myzset", {"one": 1}) == 0
    assert redis_client.zscore("myzset", "one") == 1.0

def test_zrem(redis_client):
    redis_client.zadd("myzset2", {"one": 1, "two": 2})
    assert redis_client.zrem("myzset2", "one") == 1
    assert redis_client.zrem("myzset2", "nonexistent") == 0
    assert redis_client.zcard("myzset2") == 1

def test_zcard(redis_client):
    redis_client.zadd("myzset3", {"one": 1, "two": 2})
    assert redis_client.zcard("myzset3") == 2

def test_zrange(redis_client):
    redis_client.zadd("myzset4", {"one": 1, "two": 2, "three": 3})
    assert redis_client.zrange("myzset4", 0, -1) == ["one", "two", "three"]
    assert redis_client.zrange("myzset4", 0, -1, withscores=True) == [("one", 1.0), ("two", 2.0), ("three", 3.0)]
    assert redis_client.zrange("myzset4", 1, 2) == ["two", "three"]

def test_zrangebyscore(redis_client):
    redis_client.zadd("myzset5", {"one": 1, "two": 2, "three": 3})
    assert redis_client.zrangebyscore("myzset5", 1, 2) == ["one", "two"]
    # Assuming standard Redis zrangebyscore min max
    # redis-py: zrangebyscore(name, min, max, start=None, num=None, withscores=False, score_cast_func=float)
