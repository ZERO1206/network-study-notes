#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // 创建socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }

    // 允许端口复用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定 0.0.0.0:9999
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 开始监听
    listen(server_fd, 3);
    std::cout << "[服务器] 监听 0.0.0.0:9999\n";

    // 接受连接（阻塞等待）
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
    std::cout << "[服务器] 客户端已连接\n";

    // 接收数据
    char buf[1024];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if(n > 0) {
        buf[n] = '\0';
        std::cout << "[服务器] 收到: " << buf;
        send(client_fd, buf, n, 0);
        std::cout << "[服务器] 已回送\n"; 
    }

    close(client_fd);
    close(server_fd);
    return 0;
}