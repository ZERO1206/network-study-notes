#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    const char* msg = "Hello UDP!\n";
    sendto(sock, msg, strlen(msg), 0, (sockaddr*)&server_addr, sizeof(server_addr));
    std::cout << "[UDP客户端] 已发送: " << msg;

    char buf[1024];
    int n = recvfrom(sock, buf, sizeof(buf)-1, 0, nullptr, nullptr);
    if(n > 0) {
        buf[n] = '\0';
        std::cout << "[UDP客户端] 收到回复: " << buf;
    }

    close(sock);
    return 0;
}