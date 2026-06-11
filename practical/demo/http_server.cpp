#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <string>
#include <cstdio>

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

        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        if(FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            clients.push_back(client_fd);
            std::cout << "[HTTP] 新连接 fd=" << client_fd
                      << " (共" << clients.size() << "人在线)\n";
        }

        char buf[4096];
        for(auto it = clients.begin(); it != clients.end(); ) {
            int fd = *it;
            if(FD_ISSET(fd, &read_fds)) {
                int n = recv(fd, buf, sizeof(buf) - 1, 0);
                if(n <= 0) {
                    if(n == 0)
                        std::cout << "[HTTP] 连接断开 fd=" << fd << "\n";
                    close(fd);
                    it = clients.erase(it);
                    continue;
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

                std::cout << "[HTTP] 请求路径: " << path << "\n";

                // 映射到本地文件
                char filepath[512];
                if(strcmp(path, "/") == 0) {
                    strcpy(filepath, "www/index.html");
                }
                else {
                    snprintf(filepath, sizeof(filepath), "www%s", path);
                }

                // 尝试读文件
                FILE* fp = fopen(filepath, "rb");
                std::string response;

                if(fp == nullptr) {
                    // 文件不存在 --- 404
                    const char* body = "<h1>404 Not Found</h1>";
                    response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/html; charset=utf-8\r\n"
                                "Content-Length: "
                                + std::to_string(strlen(body)) + "\r\n"
                                + "\r\n" + body;
                }
                else {
                    // 文件存在 --- 读内容
                    fseek(fp, 0, SEEK_END);
                    long size = ftell(fp);
                    rewind(fp);
                    std::string body(size, '\0');
                    fread(&body[0], 1, size, fp);
                    fclose(fp);

                    // 根据后缀判断 Content-Type
                    const char* content_type = "application/octet-stream";
                    const char* ext = strrchr(filepath, '.');
                    if(ext != nullptr) {
                        if(strcmp(ext, ".html") == 0)
                            content_type = "text/html; charset=utf-8";
                        else if(strcmp(ext, ".css") == 0)
                            content_type = "text/css; charset=utf-8";
                        else if(strcmp(ext, ".js") == 0)
                            content_type = "application/javascript";
                        else if(strcmp(ext, ".png") == 0)
                            content_type = "image/png";
                        else if(strcmp(ext, ".jpg") == 0)
                            content_type = "image/jpeg";
                        else if(strcmp(ext, ".txt") == 0)
                            content_type = "text/plain; charset=utf-8";
                    }

                    response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: "
                            + std::string(content_type) + "\r\n"
                            + "Content-Length: " + std::to_string(size) + "\r\n"
                            + "\r\n" + body;
                }

                // 发给浏览器
                send(fd, response.c_str(), response.size(), 0);
                close(fd);
                it = clients.erase(it);
                continue;
            }
            ++it;
        }
    }

    close(server_fd);
    return 0;
}
