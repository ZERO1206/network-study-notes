# C++ TCP 客户端与服务器 —— 指导手册

> 不要复制粘贴。每步自己敲，看输出，想为什么。

---

## 任务 1：编写 TCP Echo 服务器

在当前目录（`practical/demo/`）创建 `server.cpp`，写入以下代码：

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

    // 允许端口复用
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

    close(client_fd);
    close(server_fd);
    return 0;
}
```

编译：
```bash
g++ -std=c++17 -o server server.cpp
```

---

## 任务 2：编写 TCP 客户端

创建 `client.cpp`：

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

编译：
```bash
g++ -std=c++17 -o client client.cpp
```

---

## 任务 3：自己动手跑

打开**两个终端**，都在 `practical/demo/` 目录下。

终端 1：
```bash
./server
```

终端 2：
```bash
./client
```

两个终端分别输出什么？记录下来。

---

## 任务 4：用 strace 看系统调用

```bash
strace -e trace=network ./client 2>&1
```

你会看到类似这样的输出：
```
socket(AF_INET, SOCK_STREAM, ...) = 3
connect(3, ...) = 0
sendto(3, "Hello TCP!\n", ...) = 11
recvfrom(3, ...) = 11
```

**对照你的理论：**
- `connect()` = 三次握手
- `close()` = 四次挥手（也在输出里，找找看）

---

## 任务 5：用 ss 看连接状态

终端 1 先起 server，终端 2 执行：
```bash
# 服务器起来后，看监听状态
ss -tlnp | grep 9999
```

然后起 client，在 client 跑完前（加个 `sleep` 或者用 `nc` 连上去挂着），另一个终端：
```bash
# 看已建立的连接
ss -tnp | grep 9999
```

---

## 理解确认

完成以上操作后，用自己的话回答：

1. 服务器的 `socket() → bind() → listen() → accept() → recv() → send() → close()` 七步，每一步对应什么现实动作？
2. `connect()` 为什么就是三次握手？你看 strace 输出中 connect 返回 0 意味着什么？
3. `htons(9999)` 是干什么的？为什么需要它？

把答案写在下面（或者口头说一遍）。

---

> 下一步：完成确认后进入 [strace 深入追踪](02_strace追踪系统调用.md)