#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <ctime>

#pragma comment(lib,"ws2_32.lib") // Winsock Library

#define BUFFER_SIZE 2048

HWND hChatBox, hInputBox, hSendButton, hNameInputBox, hUserListBox;
HWND hErrorLabel;  // 静态文本控件，用于显示错误提示
SOCKET sock = 0;
char name[32] = "";
char buffer[BUFFER_SIZE];


// Function prototypes
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK NameInputProcedure(HWND, UINT, WPARAM, LPARAM);
void addMessageToChatBox(const char* message);
void sendMessage();
void setupConnection();
void showNameInputDialog(HINSTANCE hInst);

// Function to create and show name input dialog
void showNameInputDialog(HINSTANCE hInst) {
    WNDCLASSA wc = {0};
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = "NameInputDialog";
    wc.lpfnWndProc = NameInputProcedure;

    RegisterClassA(&wc);

    HWND hNameWindow = CreateWindowA("NameInputDialog", "Enter Your Name", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 400, 200, 300, 200, NULL, NULL, NULL, NULL);
    
    // Add input box and button
    hNameInputBox = CreateWindowA("Edit", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 50, 50, 200, 25, hNameWindow, NULL, NULL, NULL);
    CreateWindowA("Button", "Submit", WS_VISIBLE | WS_CHILD, 100, 100, 80, 25, hNameWindow, (HMENU)1, NULL, NULL);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Name input dialog procedure
LRESULT CALLBACK NameInputProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wp) == 1) {  // Submit button clicked
                GetWindowTextA(hNameInputBox, name, 32);
                
                if (strlen(name) > 0) {
                    // 发送用户名到服务器进行检查
                    send(sock, name, strlen(name), 0);

                    // 接收服务器响应
                    char buffer[BUFFER_SIZE];
                    int receive = recv(sock, buffer, sizeof(buffer) - 1, 0);
                    buffer[receive] = '\0';  // 添加字符串结束符

                    // 检查服务器返回的消息
                    if (strcmp(buffer, "Username taken") == 0) {
                        // 显示提示框，告知用户重名错误
                        MessageBoxA(hWnd, "Username already taken! Please choose another.", "Error", MB_OK);
                    } else {
                        // 用户名可用，关闭窗口并进入聊天界面
                        DestroyWindow(hWnd);
                        PostQuitMessage(0);
                    }
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hWnd, msg, wp, lp);
    }
    return 0;
}


// Main chat window procedure
LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            // 聊天框
            hChatBox = CreateWindowA(
                "Edit", 
                "", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | 
                ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,  // 多行 + 自动滚动 + 只读
                20, 20, 340, 400, 
                hWnd, NULL, NULL, NULL
            );

            // 输入框
            hInputBox = CreateWindowA(
                "Edit", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
                20, 440, 340, 25, hWnd, NULL, NULL, NULL
            );

            // 发送按钮
            hSendButton = CreateWindowA(
                "Button", "Send", WS_VISIBLE | WS_CHILD,
                380, 440, 80, 25, hWnd, (HMENU)1, NULL, NULL
            );

            // 用户列表框
            hUserListBox = CreateWindowA(
                "ListBox", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,  // 支持滚动和通知
                380, 20, 120, 400, 
                hWnd, NULL, NULL, NULL
            );

            // 退出按钮
            CreateWindowA(
                "Button", "Exit", WS_VISIBLE | WS_CHILD,
                380, 480, 80, 25, hWnd, (HMENU)2, NULL, NULL
            );
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == 1) {  // 发送按钮被点击
                sendMessage();
            } else if (LOWORD(wp) == 2) {  // 退出按钮被点击、
                const char* quit_message = "QUIT";
                send(sock, quit_message, strlen(quit_message), 0);  // 发送退出消息
                PostQuitMessage(0);  // 关闭应用程序
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hWnd, msg, wp, lp);
    }
    return 0;
}


// Function to add messages to the chat box
void addMessageToChatBox(const char* message) {
    std::string formatted_message(message);
    
    // 如果消息没有换行符，确保添加换行符
    if (formatted_message.back() != '\n') {
        formatted_message += "\r\n";
    }

    int len = GetWindowTextLength(hChatBox);
    SendMessage(hChatBox, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hChatBox, EM_REPLACESEL, FALSE, (LPARAM)formatted_message.c_str());
}

// Function to send a message
void sendMessage() {
    char message_with_name[BUFFER_SIZE + 32];  // Ensure buffer size for message and username
    GetWindowTextA(hInputBox, buffer, BUFFER_SIZE);

     // 检查消息长度是否超限
    if (strlen(buffer) >= BUFFER_SIZE - 1) {
        MessageBoxA(NULL, "Error: Message too long.", "Error", MB_OK);
        return;  // 不发送超长消息
    }

    // Avoid sending empty messages
    if (strlen(buffer) == 0) return;

    // 检查是否是私聊命令 "/private [username] [message]"
    if (strncmp(buffer, "/private", 8) == 0) {
        // 如果是私聊命令，直接将消息发送给服务器，不加时间戳
        send(sock, buffer, strlen(buffer), 0);
    } else {
        // 普通公共消息处理：添加时间戳和用户名
        time_t now = time(0);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

        // Append username and timestamp to the message
        sprintf(message_with_name, "[%s] %s: %s", timestamp, name, buffer);

        // Send message with username and timestamp to server
        send(sock, message_with_name, strlen(message_with_name), 0);
        
        // Display locally sent message in chatbox
        addMessageToChatBox(message_with_name);
    }

    // Clear input box after sending
    SetWindowTextA(hInputBox, "");
}

DWORD WINAPI recvMessages(LPVOID lpParam) {
    char message[BUFFER_SIZE];

    while (1) {
        int receive = recv(sock, message, BUFFER_SIZE - 1, 0);  // 预留一个字节给 '\0'
        if (receive > 0) {
            message[receive] = '\0';  // 确保字符串以 '\0' 结尾

            // 检查是否为用户列表消息
            if (strncmp(message, "USER_LIST", 9) == 0) {
                // 清空用户列表框
                SendMessage(hUserListBox, LB_RESETCONTENT, 0, 0);

                // 按行解析用户列表，并添加到列表框
                char* token = strtok(message + 10, "\n");
                while (token != NULL) {
                    SendMessage(hUserListBox, LB_ADDSTRING, 0, (LPARAM)token);
                    token = strtok(NULL, "\n");
                }
            } 
            // 检查是否为私聊消息
            else if (strstr(message, "[Private]") != NULL) {
                std::string private_message(message);

                // 移除末尾的 '\r' 或 '\n'
                while (!private_message.empty() && 
                      (private_message.back() == '\n' || private_message.back() == '\r')) {
                    private_message.pop_back();
                }

                // 显示私聊消息
                addMessageToChatBox(private_message.c_str());
            } 
            // 处理公共消息
            else {
                addMessageToChatBox(message);  // 只显示公共消息
            }
        } else {
            std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }
    return 0;
}





void setupConnection() {
    WSADATA wsa;
    struct sockaddr_in server_addr;

    printf("\nInitializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxA(NULL, "Failed to initialize Winsock", "Error", MB_OK);
        exit(1);
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        MessageBoxA(NULL, "Failed to create socket", "Error", MB_OK);
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        MessageBoxA(NULL, "Failed to connect to server", "Error", MB_OK);
        exit(1);
    }

    send(sock, name, 32, 0);  // Send username to the server

    // Create a thread to receive messages
    CreateThread(NULL, 0, recvMessages, NULL, 0, NULL);
}

int main() {
    HINSTANCE hInst = GetModuleHandle(NULL);

    // Show name input dialog before proceeding to chat window
    showNameInputDialog(hInst);

    // After the user enters the name, proceed to chat window
    WNDCLASSA wc = {0};
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = "ChatApp";
    wc.lpfnWndProc = WindowProcedure;

    RegisterClassA(&wc);

    HWND hChatWindow = CreateWindowA("ChatApp", "Chat Client", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 500, 600, NULL, NULL, NULL, NULL);

    setupConnection();

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
