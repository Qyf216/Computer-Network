#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <pthread.h>
#include <ctime> 
#include <fstream>
#pragma comment(lib,"ws2_32.lib") // Winsock Library

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
std::ofstream log_file;  // 日志文件

// 初始化日志文件
void init_log() {
    log_file.open("chat_server_log.txt", std::ios::app);  // 追加模式
    if (log_file.is_open()) {
        log_file << "Server started at: " << GetCurrentTime() << std::endl;
    } else {
        std::cerr << "Unable to open log file!" << std::endl;
    }
}

// 记录日志
void log_message(const std::string& message) {
    if (log_file.is_open()) {
        log_file << message;
    }
}

// 关闭日志文件
void close_log() {
    if (log_file.is_open()) {
        log_file << "Server stopped at: " << GetCurrentTime() << std::endl;
        log_file.close();
    }
}
// 客户端结构体
struct Client {
    sockaddr_in addr;  // 客户端地址
    SOCKET sock;       // 客户端套接字
    int uid;           // 客户端唯一ID
    char name[32];     // 客户端用户名
};

// 全局客户端列表
std::vector<Client*> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int uid = 10;  // 全局唯一ID生成

// 添加客户端到列表
void add_client(Client *cl) {
    pthread_mutex_lock(&clients_mutex);
    clients.push_back(cl);
    pthread_mutex_unlock(&clients_mutex);
}

// 从列表中删除客户端
void remove_client(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (size_t i = 0; i < clients.size(); ++i) {
        if (clients[i]->uid == uid) {
            clients.erase(clients.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// 向所有客户端发送消息
void send_message(const std::string &message, int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (const auto &client : clients) {
        if (client->uid != uid) {
            send(client->sock, message.c_str(), message.size(), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // 记录到日志
    log_message(message);
}


// 向指定客户端发送私聊消息
void send_private_message(const std::string &message, const char *recipient_name) {
    pthread_mutex_lock(&clients_mutex);
    for (const auto &client : clients) {
        if (strcmp(client->name, recipient_name) == 0) {
            send(client->sock, message.c_str(), message.size(), 0);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void updateUserList() {
    std::string user_list_message = "USER_LIST\n";

    pthread_mutex_lock(&clients_mutex);  // 确保线程安全
    for (const auto& client : clients) {
        user_list_message += client->name;
        user_list_message += "\n";  // 每个用户名换行
    }
    pthread_mutex_unlock(&clients_mutex);  // 解锁

    // 将用户列表消息广播给所有客户端
    for (const auto& client : clients) {
        send(client->sock, user_list_message.c_str(), user_list_message.size(), 0);
    }
}



// 获取当前时间并格式化为字符串
std::string get_current_time() {
    time_t now = time(0);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));  // 格式化时间为 YYYY-MM-DD HH:MM:SS
    return std::string(timestamp);
}

void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    std::string message;
    Client *cli = (Client *)arg;
    int leave_flag = 0;

    // 1. 使用临时变量接收用户名
    char temp_name[32] = {0};  // 初始化为 0
    int receive = recv(cli->sock, temp_name, sizeof(temp_name) - 1, 0);  // 接收用户名并保留最后一个字节给 '\0'

    if (receive <= 0 || strlen(temp_name) < 1) {
        std::cout << "Failed to get username." << std::endl;
        closesocket(cli->sock);
        delete cli;
        pthread_detach(pthread_self());
        return nullptr;
    }

    // 2. 检查是否存在重复用户名
    bool name_taken = false;

    pthread_mutex_lock(&clients_mutex);  // 确保线程安全
    for (const auto& client : clients) {
        if (strcmp(client->name, temp_name) == 0) {  // 如果用户名已存在
            name_taken = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);  // 解锁

    if (name_taken) {
        const char *error_message = "Username taken";
        send(cli->sock, error_message, strlen(error_message), 0);

        std::cout << "Username " << temp_name << " is already taken." << std::endl;

        // 确保立即断开连接并释放资源
        closesocket(cli->sock);
        remove_client(cli->uid);  // 如果用户已加入列表，确保将其移除
        updateUserList(); 
        delete cli;
        pthread_detach(pthread_self());
        return nullptr;  // 返回，避免后续代码执行
    }
    // 3. 如果用户名不重复，将用户名复制到客户端结构体中
    strncpy(cli->name, temp_name, sizeof(cli->name) - 1);

    // 4. 添加客户端到列表
    add_client(cli);
    updateUserList(); 
    

    // 5. 通知其他客户端新用户加入
    std::string current_time = get_current_time();
    message = "[" + current_time + "] " + std::string(cli->name) + " has joined the chatroom.\r\n";
    std::cout << message;
    send_message(message, cli->uid);


    while (true) {
        if (leave_flag) {
            break;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int receive = recv(cli->sock, buffer, BUFFER_SIZE, 0);

        if (receive > 0) {
            if (strlen(buffer) > 0) {
                std::string received_message(buffer);
                // 检查是否为 "QUIT" 消息
                if (strcmp(buffer, "QUIT") == 0) {
                    std::string current_time = get_current_time();
                    message = "[" + current_time + "] " + std::string(cli->name) + " has left the chatroom.\r\n";
                    std::cout << message;
                    send_message(message, cli->uid);
                    leave_flag = 1;  // 标记为正常退出
                    break;
                }
                // 处理私聊命令 "/private [username] [message]"
                if (received_message.find("/private") == 0) {
                    size_t first_space = received_message.find(" ");
                    size_t second_space = received_message.find(" ", first_space + 1);

                    if (first_space != std::string::npos && second_space != std::string::npos) {
                        std::string current_time = get_current_time();
                        std::string recipient_name = received_message.substr(first_space + 1, second_space - first_space - 1);
                        std::string private_message = received_message.substr(second_space + 1);
                        private_message = "[" + current_time + "] " + "[Private] " + std::string(cli->name) + ": " + private_message + "\r\n";
                        send_private_message(private_message, recipient_name.c_str());
                    } else {
                        std::string error_message = "Incorrect private message format. Use: /private [username] [message]\r\n";
                        send(cli->sock, error_message.c_str(), error_message.size(), 0);
                    }
                } else {
                    // 公共消息处理
                    std::string forward_message = std::string(buffer) + "\r\n";
                    send_message(forward_message, cli->uid);
                    std::cout << forward_message;
                }
            }
        } else if (receive == 0) {  // 客户端正常断开
            std::string current_time = get_current_time();
            message = "[" + current_time + "] " + std::string(cli->name) + " has left the chatroom.\r\n";
            std::cout << message;
            send_message(message, cli->uid);
            leave_flag = 1;
            break;
        } 
        else {  // 异常断开
            if (WSAGetLastError() == WSAECONNRESET) {
                std::string current_time = get_current_time();
                message = "[" + current_time + "] " + std::string(cli->name) + " has abruptly left the chatroom.\r\n";
                std::cout << message;
                send_message(message, cli->uid);
            } else {
                std::cerr << "Failed to receive message: " << WSAGetLastError() << std::endl;
            }
            leave_flag = 1;
            break;
        }
    }

    closesocket(cli->sock);
    remove_client(cli->uid);
    updateUserList(); 
    delete cli;
    pthread_detach(pthread_self());

    return nullptr;
}




int main() {
    WSADATA wsa;
    SOCKET listen_sock, client_sock;
    sockaddr_in server_addr, client_addr;
    pthread_t tid;

    std::cout << "Initializing Winsock..." << std::endl;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Failed to initialize Winsock. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 初始化日志文件
    init_log();

    // 创建监听套接字
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "Could not create socket: " << WSAGetLastError() << std::endl;
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    // 绑定
    if (bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 监听
    if (listen(listen_sock, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    std::cout << "=== Server started, waiting for clients to connect... ===" << std::endl;

    while (true) {
        int c = sizeof(sockaddr_in);
        client_sock = accept(listen_sock, (sockaddr*)&client_addr, &c);

        if (client_sock == INVALID_SOCKET) {
            std::cerr << "Accept failed. Error Code: " << WSAGetLastError() << std::endl;
            continue;
        }

        if (clients.size() >= MAX_CLIENTS) {
            std::cerr << "Max clients reached. Rejecting connection..." << std::endl;
            closesocket(client_sock);
            continue;
        }

        // 初始化客户端
        Client *cli = new Client;
        cli->addr = client_addr;
        cli->sock = client_sock;
        cli->uid = uid++;

        //add_client(cli);
        pthread_create(&tid, nullptr, &handle_client, (void*)cli);
    }

    // 关闭日志文件
    close_log();

    closesocket(listen_sock);
    WSACleanup();

    return 0;
}
