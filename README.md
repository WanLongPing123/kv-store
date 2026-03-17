# kv-store
c++ kv(key &amp; value) store with net and concurrency

[中文版](README-zh.md)

A simple but robust key-value storage engine written in C++. It supports concurrent network requests, persistent storage (AOF), and multi-threaded client testing.

## Features

- **In-memory storage**: Thread-safe `std::map` protected by `std::mutex`.
- **Network server**: TCP server (Windows sockets) handling multiple clients.
- **Command parsing**: Supports `set key value` and `get key` commands.
- **Persistent storage (AOF)**: All `set` operations are appended to `data.txt`. On startup, the file is reloaded to restore previous state.
- **Sticky packet handling**: Uses a `leftover` buffer to assemble incomplete commands.
- **Concurrency test**: Multi-threaded client simulator to verify correctness under load.

## Getting Started

### Prerequisites

- Windows OS (uses Winsock2)
- C++17 compiler (Visual Studio or MinGW)

### Compilation

Clone the repository and compile the source:

```bash
git clone https://github.com/WanLongPing123/kv-store.git
cd kv-store
g++ -std=c++17 -pthread main.cpp -o kvstore.exe -lws2_32
```

## Usage

### Run the server (default port 8888):

```bash
kvstore.exe server
```

### In another terminal, run a client test:

```bash
kvstore.exe client
```

### Or run both server and client in the same process (for testing):

```bash
kvstore.exe
```
## Example Commands

### You can also use `telnet` to connect manually:

```bash
telnet 127.0.0.1 8888
set name alice
OK
get name
alice
```

## Design Overview

- `KVStore`: Base class with `std::map` and a mutex. Provides `set` and `get` operations.

- `PersistentKVStore`: Inherits from `KVStore`, adds AOF persistence. On construction, it loads existing records from `data.txt`. Each `set` appends the command to the file.

- `KVServer`: Manages a listening socket, accepts clients, and spawns a thread per client. Handles TCP stream fragmentation and delegates commands to `PersistentKVStore`.

- Client test: Spawns multiple threads, each sending `set` and `get` commands to shared keys, measuring total time and verifying final values.

## Performance

Basic test with 10 threads, 50 operations per thread (mixed set/get) on the same machine:

- **Total time**: XXX ms (fill in your measured value)

## Future Improvements
- **Fine-grained locking**: Replace single mutex with sharded locks or read-write locks to improve concurrency.

- **Command length limit**: Protect against malicious clients by limiting maximum command size.

- **AOF rewrite**: Compact the append-only file by writing only the latest value of each key.

- **LRU eviction**: Automatically remove least recently used keys when memory limit is reached.

- **Cross-platform support**: Abstract socket and threading to support Linux/macOS.

## Contributing
Feel free to open issues or pull requests if you have suggestions or find bugs.

## License
This project is open source and available under the [MIT License](LICENSE).