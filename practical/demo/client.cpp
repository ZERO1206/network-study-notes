// TCP 客户端 —— 连接服务器，发一句话，收回复
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // 1. 创建 socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    // 2. connect —— 打给服务器（三次握手在这里发生！）
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, (sockaddr*)&addr, sizeof(addr));
    std::cout << "[客户端] 已连接到服务器\n";

    // 3. send —— 发数据
    const char* msg = "Hello TCP!\n";
    send(sock, msg, strlen(msg), 0);
    std::cout << "[客户端] 已发送: " << msg;

    // 4. recv —— 收回复
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[客户端] 收到回复: " << buf;
    }

    close(sock);
    return 0;
}