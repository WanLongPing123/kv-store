#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// 发送命令（与 client.cpp 中相同）
std::string send_command(const std::string& server_ip, int port, const std::string& cmd) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "WSAStartup failed";
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return "socket error";
    }
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &server.sin_addr);
    if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return "connect error";
    }
    std::string send_cmd = cmd + "\n";
    send(sock, send_cmd.c_str(), send_cmd.size(), 0);
    char buffer[1024] = {0};
    int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
    std::string response;
    if (len > 0) {
        buffer[len] = '\0';
        response = buffer;
    }
    closesocket(sock);
    WSACleanup();
    return response;
}

// 共享的 key 集合
const std::vector<std::string> SHARED_KEYS = {"shared_key_A", "shared_key_B", "shared_key_C"};

// 客户端线程函数：对共享 key 进行多次 set 操作
void client_thread(int thread_id, int num_ops) {
    for (int i = 0; i < num_ops; ++i) {
        std::string key = SHARED_KEYS[i % SHARED_KEYS.size()];
        std::string value = "thread" + std::to_string(thread_id) + "_op" + std::to_string(i);
        std::string set_resp = send_command("127.0.0.1", 8888, "set " + key + " " + value);
        if (set_resp.find("OK") == std::string::npos) {
            std::cout << "Thread " << thread_id << " set failed: " << set_resp << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    int num_threads = 10;
    int ops_per_thread = 50;

    if (argc >= 2) num_threads = std::stoi(argv[1]);
    if (argc >= 3) ops_per_thread = std::stoi(argv[2]);

    std::cout << "Starting concurrent test with " << num_threads << " threads, "
              << ops_per_thread << " ops per thread.\n";

    std::vector<std::thread> threads;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(client_thread, i, ops_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "All threads finished in " << ms << " ms.\n";

    // 查询共享 key 的最终值
    std::cout << "\nFinal values of shared keys:\n";
    for (const auto& key : SHARED_KEYS) {
        std::string val = send_command("127.0.0.1", 8888, "get " + key);
        std::cout << key << " = " << val;  // val 自带换行符
    }

    return 0;
}