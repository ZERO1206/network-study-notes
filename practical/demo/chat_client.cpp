#include <iostream>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/select.h>
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

    std::cout << "[聊天室] 已连接! 输入消息回车发送(输入 /quit 退出)\n";

    while(true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);            // 盯服务器消息
        FD_SET(STDIN_FILENO, &read_fds);    // 盯键盘输入
        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        // 情况1:服务器发消息来了
        if(FD_ISSET(sock, &read_fds)) {
            char buf[1024];
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if(n <= 0) {
                std::cout << "[聊天室] 连接断开\n";
                break;
            }
            buf[n] = '\0';
            std::cout << "[消息] " << buf;
        }

        // 情况2:敲了键盘
        if(FD_ISSET(STDIN_FILENO, &read_fds)) {
            std::string line;
            std::getline(std::cin, line);
            if(line == "/quit") break;
            line += '\n';
            send(sock, line.c_str(), line.size(), 0);
        }
    }

    close(sock);
    return 0;
}
