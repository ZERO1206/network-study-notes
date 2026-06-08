# 第三课：C++ UDP 通信

---

## UDP vs TCP —— 代码层面的差异

```
TCP（第一课）：                          UDP（本课）：
socket() → bind() → listen()            socket() → bind()
  → accept() → send()/recv()              → sendto()/recvfrom()
  → close()                               → close()

多出来的 listen/accept 就是"面向连接"的代价
```

| | TCP | UDP |
|------|-----|------|
| 创建 socket | `SOCK_STREAM` | `SOCK_DGRAM` |
| 建立连接 | `connect()` + `listen()` + `accept()` | **不需要** |
| 发送 | `send(fd, ...)` — 不指定目标 | `sendto(fd, ..., &addr, len)` — 每次指定发给谁 |
| 接收 | `recv(fd, ...)` — 不关心来源 | `recvfrom(fd, ..., &addr, &len)` — 每次获取谁发的 |
| 关闭 | `close()` — 四次挥手 | `close()` — 直接释放，无挥手 |

---

## 一、服务器端

### 完整代码

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // 1. 创建 UDP socket（SOCK_DGRAM）
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 2. 绑定地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(sock, (sockaddr*)&addr, sizeof(addr));

    std::cout << "[UDP服务器] 监听 0.0.0.0:9999\n";

    // 3. 直接收数据 —— 没有 listen/accept！
    char buf[1024];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                     (sockaddr*)&client_addr, &client_len);

    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[UDP服务器] 收到: " << buf;

        // 4. 回送 —— 用 recvfrom 记录的客户端地址
        sendto(sock, buf, n, 0,
               (sockaddr*)&client_addr, client_len);
        std::cout << "[UDP服务器] 已回送\n";
    }

    close(sock);
    return 0;
}
```

编译：`g++ -std=c++17 -o udp_server udp_server.cpp`

---

## 二、客户端

### 完整代码

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // 1. 创建 UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 2. 填服务器地址
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. 直接发数据 —— 没有 connect()！
    const char* msg = "Hello UDP!\n";
    sendto(sock, msg, strlen(msg), 0,
           (sockaddr*)&server_addr, sizeof(server_addr));
    std::cout << "[UDP客户端] 已发送: " << msg;

    // 4. 收回复
    char buf[1024];
    int n = recvfrom(sock, buf, sizeof(buf) - 1, 0, nullptr, nullptr);
    //                                     不关心谁回的 → nullptr
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[UDP客户端] 收到回复: " << buf;
    }

    close(sock);
    return 0;
}
```

编译：`g++ -std=c++17 -o udp_client udp_client.cpp`

---

## 三、逐段详解

### 核心区别：`SOCK_DGRAM` 替代 `SOCK_STREAM`

```cpp
int sock = socket(AF_INET, SOCK_DGRAM, 0);
//                         ↑ DGRAM = Datagram（数据报）= UDP
//              TCP 这里是 SOCK_STREAM
```

`socket()` 的第二个参数决定了整个通信模型——`SOCK_STREAM` = 有连接、可靠、有序 TCP，`SOCK_DGRAM` = 无连接、不可靠、可能乱序 UDP。其他参数完全一致。

---

### `sendto()` — 每次发送都要指定目标

```cpp
sendto(sock, msg, strlen(msg), 0,
       (sockaddr*)&server_addr, sizeof(server_addr));
```

| 参数 | 含义 |
|------|------|
| `sock` | 用哪个 socket 发 |
| `msg` | 发什么数据 |
| `strlen(msg)` | 发多少字节（消息长度用 strlen，不是 sizeof） |
| `0` | flag，一般填 0 |
| `&server_addr` | **发给谁** — 对方的 IP + 端口 |
| `sizeof(server_addr)` | 地址结构体大小 |

**和 TCP `send()` 的区别：**

```
TCP:  send(sock, buf, len, 0);              ← fd 已绑定对方，直接发
UDP: sendto(sock, buf, len, 0, &addr, len);  ← 每次指定目标
```

**`sendto()` 不报错不代表对方收到了：** UDP 无连接，`sendto()` 只把数据丢进内核发送队列就返回——不管对方在不在、端口有没有人监听。就算服务器根本没跑，`sendto()` 也照常成功。

---

### `recvfrom()` — 每次接收都要获取来源

```cpp
int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                 (sockaddr*)&client_addr, &client_len);
```

| 参数 | 方向 | 含义 |
|------|------|------|
| `sock` | 入 | 从哪个 socket 收 |
| `buf` | 出 | 数据读到哪 |
| `sizeof(buf) - 1` | 入 | 最多读多少（给 `\0` 留 1 字节） |
| `0` | 入 | flag |
| `&client_addr` | **出** | 内核填写"谁发来的"（IP + 端口） |
| `&client_len` | **入+出** | 结构体大小 → 内核写实际大小 |

**和 TCP `recv()` 的区别：**

```
TCP:  recv(sock, buf, len, 0);                 ← fd 已绑定对方
UDP: recvfrom(sock, buf, len, 0, &addr, &len);  ← 每次问"谁发的"
```

**类比：**

```
TCP recv = 接通的电话，听筒拿起来，声音肯定是对面那个人说的

UDP recvfrom = 传达室值班，谁都能往你桌上扔纸条
              每张纸条都得看落款才知道谁写的
```

---

### 后两个参数何时填 `nullptr`

**客户端填 `nullptr`：**

```cpp
recvfrom(sock, buf, len, 0, nullptr, nullptr);
```

客户端只发了一条消息给服务器，预期回复一定来自服务器，不需要确认来源。填 `nullptr` 表示"我不关心谁回的"。

**服务器不能填 `nullptr`：** 服务器需要知道客户端地址，才能用 `sendto()` 把数据回送过去。地址就是从 `recvfrom` 的 `client_addr` 拿到的。

---

### UDP 没有 listen/accept

```cpp
// TCP 服务器：
listen(server_fd, 3);                          // ← UDP 没有这步
int client_fd = accept(server_fd, ...);         // ← UDP 没有这步

// UDP 服务器：
bind(sock, ...)
// bind 完直接就能 recvfrom —— 谁往这个端口发数据都能收到
```

"连接"这个概念在 UDP 里根本不存在。`bind()` 之后这个端口就归你了，任何人往这个端口发包你都能收到。

---

### UDP 没有四次挥手

```cpp
close(sock);   // UDP：直接释放 socket，不发 FIN
               // TCP：触发 FIN → ACK → FIN → ACK
```

UDP 无连接，`close()` 只是释放本地资源，不产生任何网络流量。

---

## 四、TCP vs UDP 函数对照总表

| 步骤 | TCP | UDP | 差异原因 |
|------|-----|-----|---------|
| 创建 | `socket(AF_INET, SOCK_STREAM, 0)` | `socket(AF_INET, SOCK_DGRAM, 0)` | 第二个参数选协议 |
| 绑定 | `bind()` | `bind()` | 完全相同 |
| 监听 | `listen()` | **无** | UDP 无连接 |
| 接受 | `accept()` | **无** | UDP 无连接 |
| 发送 | `send(sock, ...)` | `sendto(sock, ..., &addr, len)` | UDP 每次指定目标 |
| 接收 | `recv(sock, ...)` | `recvfrom(sock, ..., &addr, &len)` | UDP 每次获取来源 |
| 关闭 | `close()` + 四次挥手 | `close()` 直接释放 | UDP 无连接无挥手 |

---

## 五、完整时序图

```
UDP 服务器                          UDP 客户端
─────────                          ─────────
socket(SOCK_DGRAM)                  socket(SOCK_DGRAM)
  │                                    │
bind() ← 登记 IP:Port                 │
  │                                    │
  │                                    │
  │    ←── "Hello UDP!\n" ──────── sendto() ← 直接发，没有握手
recvfrom() ← 收到                      │
  │     也知道对方地址了              │
  │                                    │
sendto() ── "Hello UDP!\n" ──→  recvfrom() ← 收到回复
  │                                    │
close()                             close() ← 直接关，没有挥手
```

**对比 TCP 少了什么：** 没有 SYN/SYN+ACK/ACK 三个握手包，没有 FIN/ACK 四个挥手包。发送方一发出去接收方就能收，中间没有连接建立的开销——这就是为什么 UDP 快。

---

## 六、任务清单

### 任务 1：编译运行

```bash
cd /home/music1206/projects/network-study/practical/demo
g++ -std=c++17 -o udp_server udp_server.cpp
g++ -std=c++17 -o udp_client udp_client.cpp
```

**终端 1：** `./udp_server`
**终端 2：** `./udp_client`

### 任务 2：先跑客户端试试

**服务器不启动**，直接跑客户端。`sendto()` 会报错吗？`recvfrom()` 会怎样？

预期：`sendto()` "成功"不报错（UDP 发了不管），但 `recvfrom()` 永远阻塞（没人回复）。

### 任务 3：strace 对比 UDP vs TCP

```bash
strace -e trace=network ./udp_client 2>&1
```

对比第一课 TCP 客户端的 strace 输出，少了哪个调用？

<details>
<summary>点击看答案</summary>

```
UDP: socket() → sendto() → recvfrom() → close()
TCP: socket() → connect() → sendto() → recvfrom() → close()
                           ↑ UDP 少了这个
```
`connect()` = 三次握手。UDP 没有连接，自然没有这步。

</details>

---

## 七、理解确认

1. UDP 代码和 TCP 代码最大的三个区别是什么？
2. `sendto()` 为什么需要后两个参数而 `send()` 不需要？
3. `recvfrom()` 的后两个参数什么时候填 `nullptr`？什么时候不能？
4. UDP 客户端先跑（服务器没开），`sendto()` 报错吗？为什么？
5. UDP 的 `close()` 和 TCP 的 `close()` 有什么不同？

---

> 所有函数的完整参数表见 [Socket API 函数速查手册](socket-api-reference.md)