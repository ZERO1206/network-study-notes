#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>

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

    int epoll_fd = epoll_create1(0);    // 创建管家

    struct epoll_event ev;      // 准备事件结构体
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);     // 注册server_fd

    struct epoll_event events[1024];    // 接收数组

    while(true) {
        int n = epoll_wait(epoll_fd, events, 1024, -1);

        for(int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if(fd == server_fd) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);     // 注册新客户端
                clients.push_back(client_fd);
                std::cout << "[聊天室] 新用户 fd=" << client_fd << "\n";
            }
            else {
                char buf[1024];
                int m = recv(fd, buf, sizeof(buf), 0);
                if(m <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    // 从clients vector 移除
                    for(auto it = clients.begin(); it != clients.end(); ++it) {
                        if(*it == fd) { clients.erase(it); break; }
                    }
                    std::cout << "[聊天室] 离开 fd=" << fd << "\n";
                }
                else {
                    buf[m] = '\0';
                    // 广播给其他人
                    for(int other : clients) {
                        if(other != fd) {
                            send(other, buf, m, 0);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
