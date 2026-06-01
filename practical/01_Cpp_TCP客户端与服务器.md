# C++ TCP 客户端与服务器

---

## 核心系统调用

```
socket()  →  创建 socket（买电话机）
bind()    →  绑定地址+端口（电话机插墙上的孔）
listen()  →  开始等电话
accept()  →  有人打进来，接听
connect() →  打给别人（三次握手在这步发生！）
send()    →  说话
recv()    →  听对方说话
close()   →  挂断（四次挥手）
```

---

## 服务器端代码

```cpp
// TCP echo 服务器 —— 收到什么就回什么
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // 1. socket —— 创建 TCP socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. setsockopt —— 允许端口复用（防止重启时绑不上）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. bind —— 绑定 0.0.0.0:9999
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // 监听所有网卡
    addr.sin_port = htons(9999);          // 端口 9999（htons = 主机序→网络序）
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 4. listen —— 开始等连接
    listen(server_fd, 3);

    // 5. accept —— 接电话（阻塞，直到有人连进来）
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);

    // 6. recv / send —— 收发数据
    char buf[1024];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        send(client_fd, buf, n, 0);   // 原样回送
    }

    close(client_fd);
    close(server_fd);
}
```

---

## 客户端代码

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // 1. socket —— 创建 TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    // 2. connect —— 连接服务器（三次握手在这里！）
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, (sockaddr*)&addr, sizeof(addr));

    // 3. send —— 发送数据
    const char* msg = "Hello TCP!\n";
    send(sock, msg, strlen(msg), 0);

    // 4. recv —— 接收回复
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "收到: " << buf;
    }

    close(sock);
}
```

---

## strace 追踪系统调用

```bash
strace -e trace=network ./client
```

输出：
```
socket(AF_INET, SOCK_STREAM, IPPROTO_IP) = 3          ← 创建 socket
connect(3, {sa_family=AF_INET, sin_port=htons(9999), ...}, 16) = 0  ← 三次握手
sendto(3, "Hello TCP!\n", 11, 0, NULL, 0) = 11        ← 发送
recvfrom(3, "Hello TCP!\n", 1023, 0, NULL, NULL) = 11  ← 接收
```

**connect() 这个系统调用就是三次握手。**

---

## 编译与运行

```bash
g++ -std=c++17 -o server server.cpp
g++ -std=c++17 -o client client.cpp

# 终端1
./server

# 终端2
./client
```

---

## ss 查看连接状态

```bash
# 查看监听端口
ss -tlnp | grep 9999
# LISTEN 0.0.0.0:9999    ← 正在监听

# 连接建立后查看
ss -tnp | grep 9999
# ESTAB 127.0.0.1:9999  127.0.0.1:xxxxx  ← 已建立连接
```

---

## 关键点回顾

| 调用 | 对应 TCP 概念 | 方向 |
|------|-------------|------|
| connect() | 三次握手 | 客户端 |
| accept() | 三次握手完成后拿连接 | 服务器 |
| send() / recv() | ACK/序号/重传/流量控制（内核代劳） | 双向 |
| close() | 四次挥手 | 双向 |
| htons() | 主机字节序 → 网络字节序（大端） | 必须调 |