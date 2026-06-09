#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "[服务室服务器] 监听 0.0.0.0:9999\n";

    std::vector<int> clients;  // 存储所有已连接客户端

    while(true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        // 把所有客户端fd加入监控集合
        for(int fd : clients) {
            FD_SET(fd, &read_fds);
            if(fd > max_fd) max_fd = fd;
        }

        // select阻塞等待
        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        // 1、先检查是否有新的连接
        if(FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            clients.push_back(client_fd);
            std::cout << "[聊天室] 新用户加入 fd=" << client_fd
                      << " (共" << clients.size() << "人在线)\n";
        }

        // 2、逐个检查已有客户端是否有消息
        char buf[1024];
        for(auto it = clients.begin(); it != clients.end(); ) {
            int fd = *it;
            if(FD_ISSET(fd, &read_fds)) {
                int n = recv(fd, buf, sizeof(buf) - 1, 0);
                if(n <= 0) {
                    // 客户端断开
                    if(n == 0)
                        std::cout << "[聊天室] 用户离开 fd=" << fd << "\n";
                    close(fd);
                    it = clients.erase(it);
                    continue;
                }
                buf[n] = '\0';
                std::cout << "[聊天室] fd=" << fd << " 说: " << buf;

                // 广播给所有人(除了发送者自己)
                for(int other : clients) {
                    if(other != fd) {
                        send(other, buf, n , 0);
                    }
                }
            }
            ++it;
        }
    }

    close(server_fd);
    return 0;
}
