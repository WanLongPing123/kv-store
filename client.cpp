#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// 发送命令到服务器并返回响应
std::string send_command(const std::string& server_ip, int port, const std::string& cmd) {
    // 初始化Winsock（每次调用简单处理，也可在main中一次初始化）
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

    // 发送命令（加上换行符）
    std::string send_cmd = cmd + "\n";
    send(sock, send_cmd.c_str(), send_cmd.size(), 0);

    // 接收响应
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

int main() {
    std::string ip = "127.0.0.1";
    int port = 8888;

    std::cout << "KVStore Client\n";
    std::cout << "Commands: set <key> <value>, get <key>, exit\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        if (line == "exit") break;

        std::string response = send_command(ip, port, line);
        std::cout << response;  // 响应已自带换行符
    }
    return 0;
}