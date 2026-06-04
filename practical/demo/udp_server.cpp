#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);  // AF_INET=IPv4 SOCK_DGRAM=UDP

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(sock, (sockaddr*)&addr, sizeof(addr));

    std::cout << "[UDP服务器] 监听0.0.0.0:9999\n";

    char buf[1024];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int n = recvfrom(sock, buf, sizeof(buf)-1, 0, (sockaddr*)&client_addr, &client_len);

    if(n > 0) {
        buf[n] = '\0';
        std::cout << "[UDP服务器] 收到来自客户端的数据: " << buf;

        sendto(sock, buf, n, 0, (sockaddr*)&client_addr, client_len);
        std::cout << "[UDP服务器] 已回送\n";
    }

    close(sock);
    return 0;
}