#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <sys/sendfile.h>
#include <cerrno>   // errno 全局错误码
#include <fcntl.h>
#include <sys/stat.h>

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);  // 浏览器默认访问这个端口
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "[HTTP服务器] 监听0.0.0.0:8888\n";
    std::vector<int> clients;

    while(true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for(int fd : clients) {
            FD_SET(fd, &read_fds);
            if(fd > max_fd) max_fd = fd;
        }

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        select(max_fd + 1, &read_fds, nullptr, nullptr, &tv);

        if(FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            set_nonblocking(client_fd);
            clients.push_back(client_fd);
            std::cout << "[HTTP] 新连接 fd=" << client_fd
                      << " (共" << clients.size() << "人在线)\n";
        }

        char buf[4096];
        for(auto it = clients.begin(); it != clients.end(); ) {
            int fd = *it;
            if(FD_ISSET(fd, &read_fds)) {
                int n = recv(fd, buf, sizeof(buf) - 1, 0);
                if(n == 0) {
                    // 对方正常关闭连接
                    std::cout << "[HTTP] 连接断开 fd=" << fd << "\n";
                    close(fd);
                    it = clients.erase(it);
                    continue;
                }
                if(n < 0) {
                    if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 非阻塞模式下暂时没数据 跳过不处理 EAGAIN/EWOULDBLOCK(非阻塞socket暂时没数据可读)
                        ++it;
                        continue;
                    }
                    else if(errno == EINTR) {
                        // 被打断 重试recv EINTR(被操作系统信号打断)
                        continue;
                    }
                    else {
                        // 真错误{ECONNRESET(对方异常重置连接)、ETIMEDOUT(连接超时)}
                        std::cout << "[HTTP] 连接错误 fd=" << fd << " errno=" << errno << "\n";
                        close(fd);
                        it = clients.erase(it);
                        continue;
                    }
                }
                buf[n] = '\0';

                std::cout << "[HTTP] 收到请求:\n" << buf << std::endl;

                // 解析请求行: GET /path HTTP/1.1
                char* path = buf;
                // 跳过"GET" 找到第一个空格后 '/'
                while(*path != ' ' && *path != '\0') path++;
                path++;
                char* end = path;
                while(*end != ' ' && *end != '\0') end++;
                *end = '\0';

                // 检查Connection头部
                bool keep_alive = false;
                if(strstr(buf, "Connection: keep-alive") != nullptr) {
                    keep_alive = true;
                }

                std::cout << "[HTTP] 请求路径: " << path << "\n";

                // 路径穿越防护
                if(strstr(path, "..") != nullptr) {
                    const char* body = "<h1>403 Forbidden</h1>";
                    std::string response = "HTTP/1.1 403 Forbidden\r\n"
                                           "Content-Type: text/html; charset=utf-8\r\n"
                                           "Content-Length: "
                                           + std::to_string(strlen(body)) + "\r\n"
                                           + "\r\n" + body;
                    send(fd, response.c_str(), response.size(), 0);
                    if(keep_alive) {
                        // Keep-Alive: 不关连接 继续select等下次请求
                        std::cout << "[HTTP] keep_alive fd=" << fd << "\n";
                        ++it;
                    }
                    else {
                        close(fd);
                        it = clients.erase(it);
                    }
                    continue;
                }

                // 映射到本地文件
                char filepath[512];
                if(strcmp(path, "/") == 0) {
                    strcpy(filepath, "www/index.html");
                }
                else {
                    snprintf(filepath, sizeof(filepath), "www%s", path);
                }

                // 尝试读文件
                int file_fd = open(filepath, O_RDONLY);
                std::string response;

                if(file_fd == -1) {
                    // 文件不存在 --- 404
                    const char* body = "<h1>404 Not Found</h1>";
                    response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/html; charset=utf-8\r\n"
                                "Content-Length: "
                                + std::to_string(strlen(body)) + "\r\n"
                                + "\r\n" + body;
                }
                else {
                    // 文件描述符 + sendfile 代替 C FILE*
                    struct stat st;
                    fstat(file_fd, &st);     // 拿文件

                    // 先发HTTP头部(Content-Length = st.st_size)
                    const char* content_type = "application/octet-stream";
                    const char* ext = strrchr(filepath, '.');
                    if(ext != nullptr) {
                        if (strcmp(ext, ".html") == 0)
                            content_type = "text/html; charset=utf-8";
                        else if (strcmp(ext, ".css") == 0)
                            content_type = "text/css; charset=utf-8";
                        else if (strcmp(ext, ".js") == 0)
                            content_type = "application/javascript";
                        else if (strcmp(ext, ".png") == 0)
                            content_type = "image/png";
                        else if (strcmp(ext, ".jpg") == 0)
                            content_type = "image/jpeg";
                        else if (strcmp(ext, ".txt") == 0)
                            content_type = "text/plain; charset=utf-8";
                    }

                    char header[512];
                    int header_len = snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %ld\r\n"
                        "\r\n", content_type, st.st_size);

                    send(fd, header, header_len, 0);  // 发头部
                    sendfile(fd, file_fd, nullptr, st.st_size);  // 发文件(零拷贝)
                    close(file_fd);                               // 文件fd用完就关

                    if (keep_alive) {
                        std::cout << "[HTTP] keep-alive fd=" << fd << "\n";
                        ++it;
                    } else {
                        close(fd);
                        it = clients.erase(it);
                    }
                    continue;
                }

                // 404：文件不存在 → 发响应 → keep_alive 判断
                send(fd, response.c_str(), response.size(), 0);
                if (keep_alive) {
                    std::cout << "[HTTP] keep-alive fd=" << fd << "\n";
                    ++it;
                } else {
                    close(fd);
                    it = clients.erase(it);
                }
                continue;
            }
            ++it;
        }
    }

    close(server_fd);
    return 0;
}
