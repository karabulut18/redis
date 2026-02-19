"""
Extended string command tests: MGET, MSET, INCR on non-integer, KEYS, EXISTS, RENAME.
"""

import pytest


def test_mset_mget(redis_client):
    result = redis_client.mset({"a": "1", "b": "2", "c": "3"})
    assert result is True
    vals = redis_client.mget("a", "b", "c", "missing")
    assert vals == ["1", "2", "3", None]


def test_mget_all_missing(redis_client):
    vals = redis_client.mget("x1", "x2")
    assert vals == [None, None]


def test_incr_on_non_integer(redis_client):
    redis_client.set("str_key", "not_a_number")
    with pytest.raises(Exception):
        redis_client.incr("str_key")


def test_keys_pattern(redis_client):
    redis_client.mset({"hello": "world", "helo": "mars", "world": "earth"})
    keys = redis_client.keys("hel*")
    assert set(keys) >= {"hello", "helo"}
    # "world" should NOT match "hel*"
    assert "world" not in keys


def test_keys_all(redis_client):
    redis_client.mset({"k1": "v1", "k2": "v2"})
    keys = redis_client.keys("*")
    assert "k1" in keys
    assert "k2" in keys


def test_exists_present(redis_client):
    redis_client.set("present", "yes")
    assert redis_client.exists("present") == 1


def test_exists_absent(redis_client):
    assert redis_client.exists("definitely_not_here") == 0


def test_exists_multiple(redis_client):
    redis_client.set("e1", "v1")
    redis_client.set("e2", "v2")
    assert redis_client.exists("e1", "e2", "missing") == 2


def test_rename(redis_client):
    redis_client.set("old_key", "value")
    redis_client.rename("old_key", "new_key")
    assert redis_client.get("old_key") is None
    assert redis_client.get("new_key") == "value"


def test_rename_nonexistent(redis_client):
    with pytest.raises(Exception):
        redis_client.rename("ghost_key", "target_key")
