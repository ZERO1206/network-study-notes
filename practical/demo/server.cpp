// TCP echo 服务器 —— 收到什么就回什么
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // 1. 创建 socket（类似买一部电话机）
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { std::cerr << "socket failed\n"; return 1; }

    // 允许端口复用（防止"Address already in use"）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. bind —— 绑定地址和端口（把电话机插到墙上的插孔）
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // 监听所有网卡
    addr.sin_port = htons(9999);          // 端口 9999
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 3. listen —— 开始等电话
    listen(server_fd, 3);
    std::cout << "[服务器] 监听 0.0.0.0:9999，等待连接...\n";

    // 4. accept —— 有人打进来了，接听
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
    std::cout << "[服务器] 客户端已连接！\n";

    // 5. recv / send —— 收发数据
    char buf[1024];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[服务器] 收到 " << n << " 字节: " << buf;
        send(client_fd, buf, n, 0);
        std::cout << "[服务器] 已回送\n";
    }

    close(client_fd);
    close(server_fd);
    return 0;
}