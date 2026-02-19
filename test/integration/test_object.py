import pytest
import redis

def test_object_encoding_string(redis_client):
    # Integer
    redis_client.set("num", "100")
    assert redis_client.object("encoding", "num") == "int"

    # Embstr (short string)
    redis_client.set("short", "hello")
    assert redis_client.object("encoding", "short") == "embstr"

    # Raw (long string > 44 bytes)
    long_str = "a" * 100
    redis_client.set("long", long_str)
    assert redis_client.object("encoding", "long") == "raw"

def test_object_encoding_hash(redis_client):
    # Small hash
    redis_client.hset("myhash", "f1", "v1")
    assert redis_client.object("encoding", "myhash") == "listpack"

    # Large hash
    for i in range(130):
        redis_client.hset("largehash", f"f{i}", f"v{i}")
    assert redis_client.object("encoding", "largehash") == "hashtable"

def test_object_encoding_list(redis_client):
    redis_client.lpush("mylist", "item")
    assert redis_client.object("encoding", "mylist") == "quicklist"

def test_object_encoding_set(redis_client):
    redis_client.sadd("myset", "member")
    assert redis_client.object("encoding", "myset") == "hashtable"

def test_object_encoding_zset(redis_client):
    # Small zset
    redis_client.zadd("myzset", {"m1": 1.0})
    assert redis_client.object("encoding", "myzset") == "listpack"

    # Large zset
    for i in range(130):
        redis_client.zadd("largezset", {f"m{i}": float(i)})
    assert redis_client.object("encoding", "largezset") == "skiplist"

def test_object_encoding_none(redis_client):
    assert redis_client.object("encoding", "nonexistent") is None

def test_object_invalid_subcommand(redis_client):
    # Redis returns nil for unknown subcommands of OBJECT
    assert redis_client.object("idletime", "key") is None
