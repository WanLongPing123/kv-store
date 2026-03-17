# kv-store
C++ KV（键值）存储引擎（支持网络和并发）

[英文版](README.md)

一个用 C++ 编写的简单但健壮的键值存储引擎。它支持并发网络请求、持久化存储（AOF）和多线程客户端测试。

## 特性

- **内存存储**：由 `std::mutex` 保护的线程安全 `std::map`。
- **网络服务器**：TCP 服务器（基于 Windows 套接字），支持处理多个客户端连接。
- **命令解析**：支持 `set key value` 和 `get key` 命令。
- **持久化存储（AOF）**：所有 `set` 操作都会追加到 `data.txt` 文件中。启动时会重新加载该文件以恢复之前的状态。
- **粘包处理**：使用 `leftover` 缓冲区拼接不完整的命令。
- **并发测试**：多线程客户端模拟器，用于验证高负载下的正确性。

## 快速开始

### 前提条件

- Windows 操作系统（使用 Winsock2 库）
- C++17 编译器（Visual Studio 或 MinGW）

### 编译

克隆仓库并编译源码：

```bash
git clone https://github.com/WanLongPing123/kv-store.git
cd kv-store
g++ -std=c++17 -pthread main.cpp -o kvstore.exe -lws2_32
```

## 使用方法

### 运行服务器（默认端口 8888）：

```bash
kvstore.exe server
```

### 在另一个终端中，运行客户端测试：

```bash
kvstore.exe client
```

### 也可以在同一个进程中运行服务器和客户端（用于测试）：

```bash
kvstore.exe
```

## 示例命令

### 你也可以使用 `telnet` 手动连接：

```bash
telnet 127.0.0.1 8888
set name alice
OK
get name
alice
```

## 设计概述

`KVStore`：基类，包含 `std::map` 和互斥锁。提供 `set` 和 `get` 操作。

`PersistentKVStore`：继承自 `KVStore`，增加了 AOF 持久化功能。构造时会从 `data.txt` 加载已有记录，每次 `set` 操作都会将命令追加到文件中。

`KVServer`：管理监听套接字，接受客户端连接，并为每个客户端创建一个线程。处理 TCP 流分片问题，并将命令委托给 `PersistentKVStore` 执行。

Client test：创建多个线程，每个线程向共享键发送 `set` 和 `get` 命令，统计总耗时并验证最终值的正确性。

## 性能

在同一台机器上的基础测试（10 个线程，每个线程执行 50 次操作，混合 set/get）：

- **总耗时**：XXX 毫秒（填写你实测的数值）

## 未来改进

- **细粒度锁**：将单互斥锁替换为分片锁或读写锁，提升并发性能。
- **命令长度限制**：限制最大命令长度，防范恶意客户端攻击。
- **AOF 文件重写**：压缩追加式文件，仅保留每个键的最新值。
- **LRU 淘汰机制**：当达到内存限制时，自动移除最近最少使用的键。
- **跨平台支持**：抽象套接字和线程相关逻辑，支持 Linux/macOS 系统。

## 贡献指南

如果你有建议或发现 bug，欢迎提交 issue 或 pull request。

## 许可证

本项目开源，基于 [MIT 许可证](LICENSE) 发布。