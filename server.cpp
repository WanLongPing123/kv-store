#include <iostream>
#include <map>
#include <string>
#include <sstream>

//文件存储
#include <fstream>

//网络支持
#include <thread>
#include <mutex>
// Windows（如果你在 Windows 测试）
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // 链接 Winsock 库

#define FILE_NAME "data.txt"

// KV存储数据结构
class KVStore {
protected: // 将锁改为 protected，以便派生类使用
    std::map<std::string, std::string> data_; // 加下划线标记成员变量
    std::mutex mutex_;
    // 无锁版本，假设调用者已经持有锁
    void set_unlocked(const std::string& key, const std::string& value) {
        data_[key] = value;
    }
public:
    // 设置键值对, virtual支持多态
    virtual void set(const std::string& key, const std::string &value) { // 传引用避免拷贝占内存，const避免引用后修改数据
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }
    // 获取键对应的值
    virtual std::string get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }
    const std::map<std::string, std::string>& getData_() const{
        return data_;
    }
    virtual ~KVStore() = default;
};

// 可持久化KV存储，继承KV存储
class PersistentKVStore : public KVStore {
private:
    std::string filename_;
    std::ofstream ofs_; // 保持打开的文件流
public:
    PersistentKVStore(const std::string& filename) : filename_(filename) {
        // 打开文件用于追加
        ofs_.open(filename_, std::ios::app); // append追加
        if (!ofs_.is_open()) {
            std::cerr << "Warning: cannot open file for append: " << filename_ << "\n";
        }

        // 从文件加载已有数据
        std::ifstream ifs(filename_);
        if (ifs.is_open()) {
            std::string line;
            while (std::getline(ifs, line)) {
                std::istringstream iss(line);
                std::string cmd, key, value;
                iss >> cmd >> key >> value;
                if (cmd == "set") {
                    KVStore::set(key, value);
                }
            }
        }
    }
    void set(const std::string& key, const std::string& value) {
        // 用一个锁保护内存和文件操作（基类锁已保护内存，但我们还需要保护文件写）
        // 注意：基类的 set 内部已经加锁，这里如果直接调用会释放锁后再写文件，不安全。
        // 解决方案：自己加锁，并同时更新内存和文件。
        std::lock_guard<std::mutex> lock(mutex_); // 使用基类的锁（需改为 protected）
        // 先写内存
        KVStore::set_unlocked(key, value);
        // 再追加到文件
        if (ofs_.is_open()) {
            ofs_ << "set " << key << " " << value << "\n";
            ofs_.flush(); // 确保写入磁盘
        }
        else {
            std::cerr << "Error: data file not open!" << std::endl;
        }
    }

    // get 方法可以直接继承，无需重写

    ~PersistentKVStore() {
        if (ofs_.is_open()) {
            ofs_.close();
        }
    }
};

// KV存储线程服务
class KVServer {
public:
    bool Start(int port) {
        // 初始化winsock
        WSADATA wsaData;									// 存储winsock的详细信息
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {	// MAKEWORD(2,2)使用2.2版本
            // 初始化失败
            std::cerr << "WSAStartup failed" << "\n";
            return false;
        }

        // 创建socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);					// AF_INET: 使用IPv4地址簇, SOCK_STREAM: 使用TCP协议, 0: 自动选择合适协议(TCP)
        if (listen_fd_ == INVALID_SOCKET) {
            std::cerr << "socket failed:" << WSAGetLastError() << "\n"; // 打印具体错误代码
            WSACleanup();												// 清理winsock资源
            return false;
        }

        // 准备地址结构
        sockaddr_in addr;					// IPv4地址结构体
        addr.sin_family = AF_INET;			// 使用IPv4
        addr.sin_port = htons(port);		// 端口号, htons: Host TO Network Short
        addr.sin_addr.s_addr = INADDR_ANY;	// 接收任何网卡的连接, 0.0.0.0

        // 绑定地址
        // (sockaddr*)&addr: 强制转换为通用地址类型
        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            // 绑定失败
            std::cerr << "bind failed" << "\n";
            closesocket(listen_fd_);
            WSACleanup(); // 清理winsock资源
            return false;
        }

        // 开始监听该线程
        // listen(): 将socket设为监听模式
        if (listen(listen_fd_, 10) == SOCKET_ERROR) { // 10: 请求队列的最大长度
            // 启动监听失败
            std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_fd_);
            WSACleanup(); // 清理winsock资源
            return false;
        }
        running_ = true; // 将线程状态设为正在运行
        try {
            accept_thread_ = std::thread(&KVServer::AcceptLoop, this); // 线程运行函数AcceptLoop()
        }
        catch (const std::exception& e) {
            // 线程启动失败
            std::cerr << "Create thread failed: " << e.what() << std::endl;
            closesocket(listen_fd_);
            WSACleanup(); // 清理winsock资源
            return false;
        }
        return true;
    }

    void Stop() {
        running_ = false;				// 通知AcceptLoop退出
        closesocket(listen_fd_);		// 关闭监听socket
        if (accept_thread_.joinable())	// 如果线程还在运行
            accept_thread_.join();		// 等待线程结束
        WSACleanup();					// 清理winsock资源
    }

private:
    void AcceptLoop() {
        while (running_) {
            SOCKET client_fd = accept(listen_fd_, nullptr, nullptr); // accept: 相当于wait操作，等待客户端连接
            if (client_fd != INVALID_SOCKET) {
                // 为客户端创建线程
                std::thread(&KVServer::HandleClient, this, client_fd).detach(); // 线程运行函数HandleClient, detach()函数让该线程独立运行
            }
        }
    }

    void HandleClient(SOCKET fd) {
        char buf[1024];
        std::string leftover;                 // 存储不完整的命令

        while (true) {
            int len = recv(fd, buf, sizeof(buf) - 1, 0);
            /*
            recv() 函数详解
            接收参数
                fd : 从哪个socket接收
                buf : 存放数据的缓冲区
                sizeof(buf) - 1 : 最大接收字节数（留一个位置给'\0'）
                0 : 没有特殊标志
            返回值 :
                ! 0 : 实际接收的字节数
                = 0 : 客户端正常关闭连接
                < 0 : 出错
            */
            if (len <= 0) break; // 连接关闭或出错

            buf[len] = '\0';					// 添加字符串结束符
            std::string data = leftover + buf;	// 合并上次剩余数据
            leftover.clear();

            // 按行分割命令（每条命令以\n结尾）
            size_t pos;
            while ((pos = data.find('\n')) != std::string::npos) { // TODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODO
                // 寻找第一行命令
                std::string cmd = data.substr(0, pos);
                data.erase(0, pos + 1); // 已执行, 清除这一行命令, 0 - pos('\n')

                if (!cmd.empty()) {
                    std::string response = ProcessRequest(cmd);
                    send(fd, response.c_str(), response.size(), 0);
                    /*
                    send() 函数详解
                        fd: 发送到哪个socket
                        response.c_str(): 要发送的数据 TODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODO
                        response.size(): 数据长度
                        0: 没有特殊标志
                    */
                }
            }
            leftover = data;	// 保存不完整的部分，下次处理
        }
        closesocket(fd);		// 关闭客户端连接
    }

    std::string ProcessRequest(const std::string& req) {
        std::istringstream iss(req);	// 创建字符串流
        std::string cmd;
        iss >> cmd;						// 读取第一个单词（命令）

        if (cmd == "set") {
            std::string key, value;
            iss >> key >> value;		// 读取键和值
            if (key.empty() || value.empty()) {
                return "ERROR: Invalid set command\nUsage: set <key> <value>\n";
            }

            kv.set(key, value);

            return "OK\n";
        }
        else if (cmd == "get") {
            std::string key;
            iss >> key;
            if (key.empty()) {
                return "ERROR: Invalid get command\nUsage: get <key>\n";
            }

            std::string value = kv.get(key);
            if (!value.empty())
                return value + "\n";
            else
                return "NOT FOUND\n";
        }

        return "ERROR: Unknown command\n";
    }

    SOCKET listen_fd_;
    bool running_ = false;
    std::thread accept_thread_;
    PersistentKVStore kv{ FILE_NAME };
};

// 并发控制版，未解决----------------------------
//class KVStore {
//public:
//	void Set(const std::string& key, const std::string& value) {
//		Shard& shard = GetShard(key);  // 根据 key 哈希到不同分片
//		shard.lock.Lock();
//		shard.data[key] = value;
//		shard.lock.Unlock();
//	}
//private:
//	static const int kShardNum = 16;
//	struct Shard {
//		Mutex lock;
//		std::map<std::string, std::string> data;
//	} shards_[kShardNum];
//
//	Shard& GetShard(const std::string& key) {
//		int h = std::hash<std::string>{}(key) % kShardNum;
//		return shards_[h];
//	}
//};
//

//}
int main()
{
    //std::cout << "Simple KVStore started. Commands: set key value | get key | exit\n";

    KVServer kvServer;
    if (kvServer.Start(8888)) {
        std::cout << "Server started on port 8888. Press Enter to stop...\n";
        std::cin.get();
        kvServer.Stop();
    }

    return 0;
}