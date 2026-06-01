# C++ TCP 客户端与服务器 —— 指导手册

> 不要复制粘贴。每步自己敲，看输出，想为什么。

---

## 服务器端完整代码

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }

    // 允许端口复用（防止重启时 "Address already in use"）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. 绑定 0.0.0.0:9999
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 3. 开始监听
    listen(server_fd, 3);
    std::cout << "[服务器] 监听 0.0.0.0:9999\n";

    // 4. 接受连接（阻塞等待）
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
    std::cout << "[服务器] 客户端已连接\n";

    // 5. 接收数据
    char buf[1024];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[服务器] 收到: " << buf;
        // 6. 回送
        send(client_fd, buf, n, 0);
        std::cout << "[服务器] 已回送\n";
    }

    // 7. 关闭
    close(client_fd);
    close(server_fd);
    return 0;
}
```

编译：`g++ -std=c++17 -o server server.cpp`

---

## 客户端完整代码

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // connect —— 三次握手在这里！
    connect(sock, (sockaddr*)&addr, sizeof(addr));
    std::cout << "[客户端] 已连接\n";

    const char* msg = "Hello TCP!\n";
    send(sock, msg, strlen(msg), 0);
    std::cout << "[客户端] 已发送: " << msg;

    char buf[1024];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[客户端] 收到回复: " << buf;
    }

    close(sock);
    return 0;
}
```

编译：`g++ -std=c++17 -o client client.cpp`

---

## 任务 1：编译运行

**终端 1：**
```bash
cd /home/music1206/projects/network-study/practical/demo
./server
```

**终端 2：**
```bash
cd /home/music1206/projects/network-study/practical/demo
./client
```

两个终端分别输出什么？

---

## 任务 2：用 strace 看系统调用

```bash
# 先起 server，再在另一个终端执行：
strace -e trace=network ./client 2>&1
```

你会看到：
```
socket(AF_INET, SOCK_STREAM, ...) = 3
connect(3, ...) = 0           ← 三次握手
sendto(3, "Hello TCP!\n", ...) = 11  ← 发送 11 字节
recvfrom(3, ...) = 11         ← 接收 11 字节
```

---

## 任务 3：用 ss 看连接状态

服务器跑起来后，另一个终端：
```bash
ss -tlnp | grep 9999     # 看 LISTEN 状态
```

客户端连接期间：
```bash
ss -tnp | grep 9999      # 看 ESTABLISHED 状态
```

---

## 理解确认

1. 服务器七步 `socket() → bind() → listen() → accept() → recv() → send() → close()` 每步的职责是什么？
2. `server_fd` 和 `client_fd` 有什么区别？为什么需要两个？
3. `connect()` 为什么等于三次握手？
4. `htons()` 不调用行不行？为什么？

---

> 所有函数详细讲解见 [Socket 函数速查手册](socket-api-reference.md)