# Redis from Scratch in C++

A high-performance, multi-threaded Redis server implementation in C++ focusing on efficiency, modern C++ patterns, and standard Redis compatibility through the RESP3 protocol.

## 🚀 Features

- **RESP3 Protocol Suite**: Full support for encoding and decoding Redis Serialization Protocol (RESP3) data types including Simple Strings, Errors, Integers, Bulk Strings, Arrays, Sets, Maps, and Booleans.
- **Enhanced Zero-Copy Architecture**: Utilizes `std::string_view` for parsing incoming network buffers and database lookups. Complements this with `SegmentedBuffer` for direct socket reads and a zero-copy persistence path.
- **Visitor Pattern Architecture**: Implements the Visitor pattern for decoupled database traversal, enabling efficient AOF rewriting, RDB snapshots, and flexible state inspection without exposing internal database structures.
- **AOF Persistence**: Full Append Only File support with configurable flush intervals (`appendfsync-interval`). Features a background rewriting mechanism (`BGREWRITEAOF`) using `fork()` COW semantics to compact logs without blocking the main event loop.
- **RDB Snapshotting**: Custom binary serialization for point-in-time snapshots. Support for both synchronous (`SAVE`) and asynchronous (`BGSAVE`) background saves.
- **Pub/Sub Messaging**: Complete publish/subscribe implementation supporting `PUBLISH`, `SUBSCRIBE`, and `UNSUBSCRIBE`. Architected with lock-free concurrent queues to ensure zero mutex contention on the messaging hot path.
- **Memory Optimized**: Employs `std::variant` for the core `RespValue` structure, significantly reducing memory overhead.
- **High-Concurrency I/O**: Multi-threaded TCP networking stack supporting `poll`-based event loops and asynchronous response queuing.
- **Lock-Free Communication**: Implements a Producer-Consumer pattern using specialized `LockFreeRingBuffer` and self-piping for efficient cross-thread signaling.

- **Supported Data Structures**:
  - **Strings**: `GET`, `SET` (with EX/PX), `DEL`, `TTL`, `PTTL`, `EXPIRE`, `PEXPIRE`, `PERSIST`, `INCR`, `INCRBY`, `DECR`, `DECRBY`, `MGET`, `MSET`, `EXISTS`, `RENAME`, `TYPE`, `KEYS`.
  - **Lists**: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LRANGE`.
  - **Hashes**: `HSET`, `HGET`, `HDEL`, `HLEN`, `HGETALL`, `HMSET`, `HMGET`.
  - **Sets**: `SADD`, `SREM`, `SISMEMBER`, `SMEMBERS`, `SCARD`.
  - **Sorted Sets**: `ZADD`, `ZREM`, `ZSCORE`, `ZRANK`, `ZRANGE`, `ZRANGEBYSCORE`, `ZCARD`.
  - **Pub/Sub**: `PUBLISH`, `SUBSCRIBE`, `UNSUBSCRIBE`.
  - **System**: `PING`, `ECHO`, `CONFIG`, `FLUSHALL`, `BGREWRITEAOF`, `SAVE`, `BGSAVE`, `CLIENT`, `OBJECT`.

## 🛠 Architecture

![Architecture](doc/architecture_diagram.png)

The project is architected for maximum performance and modularity:

- `lib/`: Core library containing networking (`TcpServer`, `TcpConnection`), the RESP parser (`RespParser`), threading utilities (`LockFreeRingBuffer`), and the Persistence engine.
- `lib/common/`: Shared utilities including `SegmentedBuffer`, `ProcessUtil` (for fork management), and intrusive data structure helpers.
- `server/`: The main server logic, implementing a Producer-Consumer architecture. The Network Thread produces commands into a lock-free queue, which the Server Thread consumes.
- `visitor/`: Implements the Visitor pattern for database traversal, separating business logic from storage format.

For a detailed deep-dive into the system design, please refer to the [Architecture Report](doc/report.pdf).

## 🏗 Building the Project

The project uses CMake for building. Ensure you have a C++17 compliant compiler installed.

```bash
mkdir -p build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## 🚦 Getting Started

### Running the Server
```bash
./build/server/redis_server
```
The server will start listening on the default Redis port (6379), load its RDB/AOF state, and begin accepting connections.

### Persistence Configuration
The server supports configurable persistence through the standard `CONFIG` command:
```bash
redis-cli CONFIG SET appendfsync-interval 5  # Flush to disk every 5 seconds
redis-cli BGREWRITEAOF                      # Trigger background compaction
```

## 🚥 Testing

The project includes a suite of unit and integration tests.

### C++ Unit Tests
```bash
# Run all core tests
./build/test/test_ringbuffer
./build/test/test_hashmap
./build/test/test_avltree
./build/test/test_database
./build/test/test_resp_parser
./build/test/test_segmented_buffer
./build/test/test_network
```

### Python Integration Tests
The integration suite utilizes `pytest` and `redis-py` to validate end-to-end command execution, persistence durability, and concurrency:
```bash
source .venv/bin/activate
pytest test/integration/ -v
```

## 📝 Future Roadmap

- [x] Persistence (AOF with Background Rewrite).
- [x] RDB Binary Export/Import.
- [x] Pub/Sub support.
- [x] Multi-key commands (`MGET`, `MSET`).
- [ ] Advanced commands (`BRPOP`, `BLPOP`, `HINCRBY`, etc.).
- [ ] Lua Scripting support.
- [ ] Redis Cluster protocol subset.


## 📚 Acknowledgments

This project was developed with the help of the excellent guide [Build Your Own Redis](https://build-your-own.org/redis/) by James Smith. Many of the architectural decisions and data structure implementations (like the intrusive hash map and AVL tree) were inspired by his approach.

## 📜 License
This project is for educational purposes as a deep dive into systems programming and networking.
