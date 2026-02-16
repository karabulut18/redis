
import pytest
import time

def test_set_get(redis_client):
    assert redis_client.set("foo", "bar") is True
    assert redis_client.get("foo") == "bar"

def test_set_overwrite(redis_client):
    redis_client.set("key", "val1")
    assert redis_client.get("key") == "val1"
    redis_client.set("key", "val2")
    assert redis_client.get("key") == "val2"

def test_del_existing_key(redis_client):
    redis_client.set("foo", "bar")
    assert redis_client.delete("foo") == 1
    assert redis_client.get("foo") is None

def test_del_nonexistent_key(redis_client):
    assert redis_client.delete("nonexistent") == 0

def test_incr(redis_client):
    redis_client.set("counter", "10")
    assert redis_client.incr("counter") == 11
    assert redis_client.get("counter") == "11"

def test_incr_new_key(redis_client):
    assert redis_client.incr("new_counter") == 1
    assert redis_client.get("new_counter") == "1"

# The following tests might be flaky if the server doesn't implement expiration precisely or if time drift occurs.
# Also, python's time vs server's time.

def test_expire(redis_client):
    redis_client.set("temp", "val")
    assert redis_client.expire("temp", 1) is True
    # Initial check
    assert redis_client.get("temp") == "val"
    time.sleep(1.2) # Wait slightly more than 1s
    assert redis_client.get("temp") is None

def test_ttl(redis_client):
    redis_client.set("timed", "val")
    redis_client.expire("timed", 10)
    ttl = redis_client.ttl("timed")
    assert 0 < ttl <= 10

def test_set_ex(redis_client):
    redis_client.set("exkey", "val", ex=1)
    assert redis_client.get("exkey") == "val"
    time.sleep(1.2)
    assert redis_client.get("exkey") is None
