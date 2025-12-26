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
- **Multi-threaded I/O**: Custom TCP networking stack supporting `poll`-based event loops and thread-per-connection models.
- **Thread Safety**: Integrated `ThreadSafeQueue` for synchronized task management.
- **Recursion Safety**: Built-in limits on nested RESP structures to prevent stack overflow attacks.
- **Structured Logging**: Clean, organized logging architecture that separates logs by application instance.

## 🛠 Architecture

The project is divided into several clear components:

- `lib/`: Core library containing networking (`TcpServer`, `TcpConnection`), the RESP parser (`RespParser`), threading utilities (`ThreadSafeQueue`), and logging.
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

## 📝 Future Roadmap

- [ ] Implementation of core data structures (`GET`, `SET`, `DEL`).
- [ ] Support for Hash Maps and Lists.
- [ ] Persistence (RDB/AOF).
- [ ] Pub/Sub support.

## 📜 License
This project is for educational purposes as a deep dive into systems programming and networking.
