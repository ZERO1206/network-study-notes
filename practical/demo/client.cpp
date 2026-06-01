#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(sock, (sockaddr*)&addr, sizeof(addr));
    std::cout << "[客户端] 已连接\n";

    const char* msg = "Hello TCP!\n";
    send(sock, msg, strlen(msg), 0);
    std::cout << "[客户端] 已发送: " << msg;

    char buf[1024];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if(n > 0) {
        buf[n] = '\0';
        std::cout << "[客户端] 收到回复: " << buf; 
    }

    close(sock);
    return 0;
}