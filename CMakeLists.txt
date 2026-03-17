cmake_minimum_required(VERSION 3.15)
project(kvStore)

set(CMAKE_CXX_STANDARD 17)

add_executable(server server.cpp
        server.cpp)
add_executable(client client.cpp
        client.cpp)
add_executable(concurrentTest concurrentTest.cpp
        concurrentTest.cpp)

# 链接 Winsock 库（必须位于 add_executable 之后）
target_link_libraries(server ws2_32)
target_link_libraries(client ws2_32)
target_link_libraries(concurrentTest ws2_32)