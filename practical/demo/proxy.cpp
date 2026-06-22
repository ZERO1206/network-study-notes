#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "[代理] 监听 0.0.0.0:8888\n";

    struct ProxyPair {
        int browser_fd;
        int target_fd;
    };
    std::vector<ProxyPair> pairs;

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (auto& p : pairs) {
            if (p.browser_fd > 0) {
                FD_SET(p.browser_fd, &read_fds);
                if (p.browser_fd > max_fd) max_fd = p.browser_fd;
            }
            if (p.target_fd > 0) {
                FD_SET(p.target_fd, &read_fds);
                if (p.target_fd > max_fd) max_fd = p.target_fd;
            }
        }

        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        // ① 浏览器连接
        if (FD_ISSET(server_fd, &read_fds)) {
            int browser_fd = accept(server_fd, nullptr, nullptr);
            if (browser_fd > 0) {
                pairs.push_back({browser_fd, -1});
                std::cout << "[代理] 浏览器连接, fd=" << browser_fd << "\n";
            }
        }

        // ② 双向转发
        char buf[8192];
        for (auto it = pairs.begin(); it != pairs.end(); ) {
            ProxyPair& p = *it;

            // 情况A：浏览器 → 目标
            if (p.browser_fd > 0 && FD_ISSET(p.browser_fd, &read_fds)) {
                int n = recv(p.browser_fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    std::cout << "[代理] 浏览器断开, fd=" << p.browser_fd << "\n";
                    close(p.browser_fd);
                    if (p.target_fd > 0) close(p.target_fd);
                    it = pairs.erase(it);
                    continue;
                }
                buf[n] = '\0';

                if (p.target_fd < 0) {
                    // 首次请求：解析 Host → DNS → connect
                    const char* host_start = strstr(buf, "Host: ");
                    if (!host_start) {
                        std::cerr << "[代理] 请求中没有 Host 头\n";
                        close(p.browser_fd);
                        it = pairs.erase(it);
                        continue;
                    }
                    host_start += 6;  // 跳过 "Host: "
                    const char* host_end = strstr(host_start, "\r\n");
                    if (!host_end) {
                        std::cerr << "[代理] Host 头格式异常\n";
                        close(p.browser_fd);
                        it = pairs.erase(it);
                        continue;
                    }
                    std::string host(host_start, host_end - host_start);

                    // 拆分 host:port
                    std::string target_host = host;
                    int target_port = 80;
                    size_t colon = target_host.find(':');
                    if (colon != std::string::npos) {
                        target_port = std::stoi(target_host.substr(colon + 1));
                        target_host = target_host.substr(0, colon);
                    }

                    // DNS 解析
                    struct hostent* he = gethostbyname(target_host.c_str());
                    if (!he) {
                        std::cerr << "[代理] DNS 失败: " << target_host << "\n";
                        close(p.browser_fd);
                        it = pairs.erase(it);
                        continue;
                    }
                    // 用 memcpy 直接拷贝 IP（避免 二进制→字符串→二进制 的冗余）
                    int target_fd = socket(AF_INET, SOCK_STREAM, 0);
                    sockaddr_in taddr{};
                    taddr.sin_family = AF_INET;
                    taddr.sin_port = htons(target_port);
                    memcpy(&taddr.sin_addr, he->h_addr_list[0], he->h_length);

                    if (connect(target_fd, (sockaddr*)&taddr, sizeof(taddr)) < 0) {
                        std::cerr << "[代理] 连接目标失败: " << target_host << "\n";
                        close(target_fd);
                        close(p.browser_fd);
                        it = pairs.erase(it);
                        continue;
                    }
                    p.target_fd = target_fd;

                    send(target_fd, buf, n, 0);
                    std::cout << "[代理] 连接目标 " << target_host << ":"
                              << target_port << ", fd=" << target_fd << "\n";
                } else {
                    send(p.target_fd, buf, n, 0);
                }
            }

            // 情况B：目标 → 浏览器
            if (p.target_fd > 0 && FD_ISSET(p.target_fd, &read_fds)) {
                int n = recv(p.target_fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    std::cout << "[代理] 目标断开, fd=" << p.target_fd << "\n";
                    close(p.target_fd);
                    close(p.browser_fd);
                    it = pairs.erase(it);
                    continue;
                }
                send(p.browser_fd, buf, n, 0);
            }

            ++it;
        }
    }
}
