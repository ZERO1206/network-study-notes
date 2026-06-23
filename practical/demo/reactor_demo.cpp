#include <iostream>
#include <vector>
#include <functional>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// 事件处理器
using EventHandler = std::function<void(int fd)>;

// Reactor:管理"fd->回调函数"的注册 + 事件循环
class Reactor {
public:
    void add_handler(int fd, EventHandler handler) {
        handlers_.push_back({fd, handler});
    }

    void run() {
        while(running_) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            int max_fd = -1;

            // 1.把所有注册的fd加入监控
            for(auto& entry : handlers_) {
                FD_SET(entry.fd, &read_fds);
                if(entry.fd > max_fd) max_fd = entry.fd;
            }

            if(max_fd < 0) break;

            // 2.等事件
            select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

            // 3.分发事件
            for(auto& entry : handlers_) {
                if(FD_ISSET(entry.fd, &read_fds)) {
                    entry.handler(entry.fd);
                }
            }
        }
    }

    void stop() { running_ = false; }

private:
    struct HandlerEntry {
        int fd;
        EventHandler handler;
    };

    std::vector<HandlerEntry> handlers_;
    bool running_ = true;
};

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)& addr, sizeof(addr));
    listen(server_fd, 10);

    Reactor reactor;
    std::vector<int> clients;

    reactor.add_handler(server_fd, [&](int fd) {
        int client_fd = accept(fd, nullptr, nullptr);
        clients.push_back(client_fd);
        std::cout << "[Reactor] 新客户端 fd=" << client_fd << "\n";

        reactor.add_handler(client_fd, [&](int cfd) {
            char buf[1024];
            int n = recv(cfd, buf, sizeof(buf)-1, 0);
            if(n <= 0) {
                close(cfd);
                return;
            }
            buf[n] = '\0';
            send(cfd, buf, n, 0);
        });
    });

    std::cout << "[Reactor] 启动 监听 9999\n";
    reactor.run();
}
