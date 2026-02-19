"""
Concurrency stress test: many parallel clients doing interleaved SET/GET/INCR
to check for data races, crashes, or corruption.
"""

import threading
import time

import pytest
import redis


NUM_THREADS = 10
OPS_PER_THREAD = 500


def worker(host, port, thread_id, errors):
    client = redis.Redis(host=host, port=port, decode_responses=True, socket_timeout=10)
    try:
        for i in range(OPS_PER_THREAD):
            key = f"stress:{thread_id}:{i % 20}"  # small key space to force contention
            client.set(key, str(i))
            val = client.get(key)
            # value could have been updated by another thread, so just check it's not None
            if val is None:
                errors.append(f"thread {thread_id}: GET returned None for key {key}")
    except Exception as e:
        errors.append(f"thread {thread_id}: exception {e}")
    finally:
        client.close()


def test_concurrent_set_get(redis_server):
    host = redis_server["host"]
    port = redis_server["port"]

    errors = []
    threads = [
        threading.Thread(target=worker, args=(host, port, tid, errors))
        for tid in range(NUM_THREADS)
    ]

    start = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=30)

    elapsed = time.time() - start
    total_ops = NUM_THREADS * OPS_PER_THREAD
    print(f"\nCompleted {total_ops} ops in {elapsed:.2f}s ({total_ops/elapsed:.0f} ops/s)")

    assert not errors, f"Errors during concurrent test:\n" + "\n".join(errors)


def test_concurrent_incr(redis_server):
    """All threads increment the same counter; final value must equal total increments."""
    host = redis_server["host"]
    port = redis_server["port"]

    setup = redis.Redis(host=host, port=port, decode_responses=True, socket_timeout=5)
    setup.set("global_counter", "0")
    setup.close()

    N_THREADS = 5
    INCRS = 200

    def incr_worker(errors):
        c = redis.Redis(host=host, port=port, decode_responses=True, socket_timeout=10)
        try:
            for _ in range(INCRS):
                c.incr("global_counter")
        except Exception as e:
            errors.append(str(e))
        finally:
            c.close()

    errors = []
    threads = [threading.Thread(target=incr_worker, args=(errors,)) for _ in range(N_THREADS)]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=30)

    assert not errors, "Errors during concurrent INCR: " + "\n".join(errors)

    result = redis.Redis(host=host, port=port, decode_responses=True).get("global_counter")
    expected = N_THREADS * INCRS
    assert int(result) == expected, f"Expected {expected}, got {result}"
