# Redis from Scratch in C++

A high-performance, multi-threaded Redis server implementation in C++ focusing on efficiency, modern C++ patterns, and standard Redis compatibility through the RESP3 protocol.

## 🚀 Features

- **RESP3 Protocol Suite**: Full support for encoding and decoding Redis Serialization Protocol (RESP3) data types:
  - Simple Strings & Errors
  - Integers & Big Numbers
  - Bulk Strings
  - Arrays & Sets
  - Maps (Key-Value pairs)
  - Booleans
- **Zero-Copy Architecture**: Utilizes `std::string_view` for parsing incoming network buffers, eliminating unnecessary heap allocations and memory copies during command processing.
- **Memory Optimized**: Employs `std::variant` for the core `RespValue` structure, significantly reducing the memory overhead of represented data types through overlapping memory storage.
- **Multi-threaded I/O**: Custom TCP networking stack supporting `poll`-based event loops.
- **Thread Safety**: Implements a lock-free Producer-Consumer pattern using `LockFreeRingBuffer` and self-piping for efficient cross-thread communication without mutex contention on the hot path.
- **Recursion Safety**: Built-in limits on nested RESP structures to prevent stack overflow attacks.
- **Structured Logging**: Clean, organized logging architecture that separates logs by application instance.

- **Supported Data Structures**:
  - **Strings**: `GET`, `SET` (with EX/PX), `DEL`, `TTL`, `EXPIRE`, `INCR`, etc.
  - **Lists**: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LRANGE`.
  - **Hashes**: `HSET`, `HGET`, `HDEL`, `HLEN`, `HGETALL`.
  - **Sets**: `SADD`, `SREM`, `SISMEMBER`, `SMEMBERS`, `SCARD`.
  - **Sorted Sets**: `ZADD`, `ZREM`, `ZSCORE`, `ZRANGE`, `ZRANGEBYSCORE`, `ZCARD`.

## 🛠 Architecture

The project is divided into several clear components:

- `lib/`: Core library containing networking (`TcpServer`, `TcpConnection`), the RESP parser (`RespParser`), threading utilities (`LockFreeRingBuffer`), and logging.
- `server/`: The main server logic, implementing a Producer-Consumer architecture. The Network Thread produces commands into a lock-free queue, which the Server Thread consumes. Responses are queued back to the Network Thread for transmission, ensuring thread safety and decoupling.
- `server/`: The main server logic, handling client state and command execution.
- `client/`: A test client utility for verifying server behavior and protocol compliance.
- `build/`: CMake build artifacts.

## 🏗 Building the Project

The project uses CMake for building. Ensure you have a C++17 compliant compiler installed.

```bash
mkdir -p build
cd build
cmake ..
make
```

## 🚦 Getting Started

### Running the Server
```bash
./build/server/redis_server
```
The server will start listening on the default Redis port (6379) and begin accepting RESP-compliant connections.

### Running the Included Client
```bash
./build/client/redis_client
```
The client connects to a hardcoded local address (127.0.0.1:6379) and periodically sends PING commands to verify connectivity and latency. Command-line argument support is planned for a future update.

### Testing with Official CLI
You can also use the standard `redis-cli` to interact with this server:
```bash
redis-cli PING
```

### Running Tests
The project includes a suite of unit and integration tests.
```bash
# Run all unit tests
./build/test/test_ringbuffer
./build/test/test_hashmap
./build/test/test_avltree
./build/test/test_database
./build/test/test_resp_parser
./build/test/test_zset
./build/test/test_network

# Run integration tests (requires python3)
python3 test_integration.py
```

## 📝 Future Roadmap

- [ ] Persistence (RDB/AOF).
- [ ] Pub/Sub support.
- [ ] Advanced commands (`BRPOP`, `HINCRBY`, `MGET`, etc.).

## 📚 Acknowledgments

This project was developed with the help of the excellent guide [Build Your Own Redis](https://build-your-own.org/redis/) by James Smith. Many of the architectural decisions and data structure implementations (like the intrusive hash map and AVL tree) were inspired by his approach.

## 📜 License
This project is for educational purposes as a deep dive into systems programming and networking.
