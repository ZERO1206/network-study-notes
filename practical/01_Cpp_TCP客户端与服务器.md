# 第一课：C++ TCP 客户端与服务器

> 不要复制粘贴。每步自己敲，看输出，想为什么。

---

## 一、服务器端

### 完整代码

```cpp
#include <iostream>       // std::cout, std::cerr
#include <cstring>        // strlen()
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), send(), recv()
#include <netinet/in.h>   // sockaddr_in, htons(), INADDR_ANY
#include <unistd.h>       // close()

int main() {
    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }

    // 2. 允许端口复用（防止重启时 "Address already in use"）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 绑定 0.0.0.0:9999
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 4. 开始监听
    listen(server_fd, 3);
    std::cout << "[服务器] 监听 0.0.0.0:9999\n";

    // 5. 接受连接（阻塞等待）
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
    std::cout << "[服务器] 客户端已连接\n";

    // 6. 接收数据 + 7. 回送
    char buf[1024];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[服务器] 收到: " << buf;
        send(client_fd, buf, n, 0);
        std::cout << "[服务器] 已回送\n";
    }

    // 8. 关闭
    close(client_fd);
    close(server_fd);
    return 0;
}
```

编译：`g++ -std=c++17 -o server server.cpp`

---

### 逐段详解

#### 头文件：为什么是这五个

| 头文件 | 提供了什么 | 代码里用在哪 |
|--------|-----------|------------|
| `<iostream>` | `std::cout`, `std::cerr` — 输出到终端 | 打印日志 |
| `<cstring>` | `strlen()` — C 风格字符串长度 | 第 1 课客户端用，服务端暂时闲置 |
| `<sys/socket.h>` | `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()` | **全部网络操作** |
| `<netinet/in.h>` | `sockaddr_in` 结构体, `htons()`, `INADDR_ANY` | IP 地址和端口的表示 |
| `<unistd.h>` | `close()` — 关闭文件描述符 | 断开连接 |

**为什么不用 `<string>` 而用 `<cstring>`？** 网络函数（`send`/`recv`）只认原始字节数组 `char[]`，不需要 C++ 的 `std::string` 对象。`<cstring>` 拿个 `strlen()` 就够了。

这些头文件属于 **POSIX 系统编程 API**，不属于 C++ 标准库。它们是操作系统提供给 C/C++ 调用内核网络功能的接口。

---

#### 第 1 步：`socket()` — 创建 socket

```cpp
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd < 0) {
    std::cerr << "socket failed\n";
    return 1;
}
```

**三个参数为什么这样填：**

| 参数 | 值 | 为什么选这个 |
|------|-----|-------------|
| 第 1 参 `AF_INET` | IPv4 地址族 | 目前绝大多数网络还是 IPv4。`AF_INET6` 是 IPv6 |
| 第 2 参 `SOCK_STREAM` | 流式 socket | = TCP。如果要写 UDP 就改成 `SOCK_DGRAM` |
| 第 3 参 `0` | 自动选择协议 | 前两个参数 `AF_INET` + `SOCK_STREAM` 已经唯一确定了 TCP/IPv4，填 0 让内核自己选 |

**返回值：**
- 非负整数 = 文件描述符（fd），内核用这个数字指代你的 socket
- 负数 = 系统资源不足，返回 -1

**为什么必须判断 `< 0`：** C 风格的系统调用不抛异常，只靠返回值报错。不检查的话后面 -1 当 fd 用会全部静默失败，根本不知道哪出了问题。

**类比：** 去营业厅办电话卡，得到一个电话号码（fd）。

---

#### 第 2 步：`setsockopt()` — 允许端口复用

```cpp
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**为什么必须写这三行？** 不写的话：跑 `./server` → Ctrl+C 关掉 → 马上重跑 → 报错 `bind: Address already in use`，得等 1~2 分钟。

**原因：** TCP 四次挥手后，主动关闭方（你的 server）进入 `TIME_WAIT` 状态，端口被内核锁定 2MSL（约 60~120 秒），防止历史残留包干扰新连接。`SO_REUSEADDR` 告诉内核："TIME_WAIT 的端口也让我 bind"。

**参数逐个解释：**

```cpp
setsockopt(
    server_fd,       // 给哪个 socket 设
    SOL_SOCKET,      // 设置层级：socket 层（不是 TCP 层）
    SO_REUSEADDR,    // 具体选项：允许地址复用
    &opt,            // 指向 int 的指针：1=开启, 0=关闭
    sizeof(opt)      // opt 占多少字节，内核需要知道读多长
);
```

**`SOL_SOCKET` vs `IPPROTO_TCP`：**
- `SOL_SOCKET`：通用 socket 层选项（地址复用、超时、缓冲区大小）
- `IPPROTO_TCP`：TCP 协议层选项（`TCP_NODELAY` 等，以后学）

`SO_REUSEADDR` 是 socket 层面的选项，所以填 `SOL_SOCKET`。

---

#### 第 3 步：`sockaddr_in` + `bind()` — 绑定地址

```cpp
sockaddr_in addr{};              // {} = C++11 零初始化
addr.sin_family = AF_INET;       // IPv4
addr.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡
addr.sin_port = htons(9999);     // 端口 9999，转网络字节序
bind(server_fd, (sockaddr*)&addr, sizeof(addr));
```

**`sockaddr_in` 结构体字段：**

| 字段 | 填什么 | 含义 |
|------|--------|------|
| `sin_family` | `AF_INET` | 这个地址是 IPv4 格式 |
| `sin_addr.s_addr` | `INADDR_ANY` | 监听本机**所有 IP**（127.0.0.1 + 192.168.x.x + ...） |
| `sin_port` | `htons(9999)` | 端口号，必须转网络字节序 |

**`INADDR_ANY` vs `127.0.0.1`：**

```
INADDR_ANY = 0.0.0.0  → 监听所有网卡，局域网其他设备也能连你
127.0.0.1  → 只监听本地环回，只能自己连自己
```

选 `INADDR_ANY`：同一台机器用 `127.0.0.1` 连可以，局域网内别的设备连你的 `192.168.1.x` 也行。

**`htons(9999)` 到底做了什么：**

你的 CPU（x86）是小端存储。`9999` = `0x270F`，在内存里：

```
小端（你的电脑）：0F 27    低位在前
大端（网络标准）：27 0F    高位在前
```

`htons()` = **H**ost **to** **N**etwork **S**hort，把主机字节序转成网络字节序（大端）。底层就一条 CPU 指令 `bswap`：`0F 27` ↔ `27 0F`。

**不转会怎样？** 你填 `9999`（`0x270F`），对方按网络序解读成 `0x0F27` = `3879`。端口对不上，`connect()` 直接失败。

**`bind()` 到底干了什么：** 在 `socket()` 和 `bind()` 之间，你的 fd 是游离的——有 TCP 协议栈但没地址。就像买了手机没插 SIM 卡。`bind()` 在操作系统协议栈里登记"IP:Port → 这个 fd"，此后所有发往 `0.0.0.0:9999` 的 TCP SYN 包都交给你。

**为什么 `(sockaddr*)` 强转：** 内核 `bind()` 的签名是通用 `sockaddr*`，要兼容 IPv4、IPv6、Unix 域等不同地址类型。`sockaddr_in` 是 IPv4 专用，传进去时强转，内核根据 `sin_family` 判断具体类型。

---

#### 第 4 步：`listen()` — 开始监听

```cpp
listen(server_fd, 3);
```

**调之前 vs 调之后：**

```
listen() 前：socket 状态 = CLOSED，有 SYN 包过来内核直接丢弃
listen() 后：socket 状态 = LISTEN，内核开始接收 SYN，自动完成三次握手
```

没调 `listen()` 就调 `accept()` 会直接失败。

**`3` 是什么：** backlog = 已完成三次握手但还没被 `accept()` 取走的连接的**最大排队数**。不是"最多连 3 个客户端"。

```
排队满之前：新 SYN → 内核完成三次握手 → 放入队列 → 等 accept() 来取
排队满之后：新 SYN → 内核直接拒绝（或忽略）
```

对于本地测试 3 够了。生产环境一般设 `SOMAXCONN`（通常 128）。

---

#### 第 5 步：`accept()` — 接受连接

```cpp
sockaddr_in client_addr{};
socklen_t len = sizeof(client_addr);
int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
```

**`server_fd` 和 `client_fd` 的区别（核心概念）：**

```
server_fd = 监听 socket，一辈子只做一件事：接新连接（accept）
            整个程序只有一个

client_fd = 通信 socket，和具体某个客户端收发数据（send/recv）
            每 accept() 一次产生一个新的 client_fd
```

**类比：** 公司前台座机（server_fd），全公司就一个，只用来接来电。接到后转你桌上的分机（client_fd），你和客户在分机上私聊。前台还能同时接第二个来电，产生第二个分机。

**参数解释：**

| 参数 | 方向 | 含义 |
|------|------|------|
| `server_fd` | 入 | 从哪个监听 socket 取连接 |
| `&client_addr` | **出** | 内核把客户端的 IP 和端口填进去 |
| `&len` | **入+出** | 入：结构体多大；出：内核写实际大小 |

**`len` 为什么传指针：** 因为不同地址族（IPv4/IPv6）返回大小可能不同，内核需要通过这个指针告知实际写入大小。

**`accept()` 是阻塞的：** 没有客户端连进来，这行代码永远不会返回，程序停在这等。

---

#### 第 6 步：`recv()` — 接收数据

```cpp
char buf[1024];
int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
```

**`sizeof(buf) - 1` 为什么减 1：** 数据读进来后要补 `\0` 当 C 字符串打印。留 1 字节给 `\0`，最多读 1023 字节数据。

**三种返回值：**

| 返回值 | 含义 |
|--------|------|
| `> 0` | 实际读到的字节数 |
| `= 0` | 对方已调用 `close()`，连接已关闭（FIN 已收到） |
| `< 0` | 出错 |

你的代码只处理了 `n > 0`，作为演示程序够用。生产代码必须处理 `n = 0`（对方关了）和 `n < 0`（错误）。

**`recv()` 也是阻塞的：** 没数据到达时程序停在这等。

---

#### 第 7 步：`send()` — 发送数据

```cpp
buf[n] = '\0';   // 先补字符串结尾
send(client_fd, buf, n, 0);   // 原样发回（echo）
```

**参数：**

| 参数 | 含义 |
|------|------|
| `client_fd` | 往哪个连接发 |
| `buf` | 数据在哪 |
| `n` | 发多少字节（实际收多少发多少，不发垃圾数据） |
| `0` | flag，一般填 0 |

**注意：** `send()` 返回只表示数据交给了内核发送缓冲区，**不代表对方已收到**。真正送达由 TCP 协议栈保证（ACK/重传/序号/流量控制——你学过的理论，内核代劳）。

---

#### 第 8 步：`close()` — 关闭连接

```cpp
close(client_fd);   // 先关通信 socket（四次挥手）
close(server_fd);   // 再关监听 socket（释放端口）
```

**`close()` 触发四次挥手：** 哪怕你代码里只写一行，内核自动完成 FIN → ACK → FIN → ACK。

**顺序：** 先关 client_fd 再关 server_fd。长期运行的服务里顺序不对可能导致 fd 泄漏。

**谁先 close，谁进 TIME_WAIT：** 服务器先 close(client_fd) → 服务器进 TIME_WAIT，端口被锁 1~2 分钟。这就是为什么需要 `SO_REUSEADDR`。

---

## 二、客户端

### 完整代码

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>    // inet_pton() —— 比 server 多这个！
#include <unistd.h>

int main() {
    // 1. 创建 socket（客户端只需要一个！）
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 填目标地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // 3. connect —— 三次握手在这里！
    connect(sock, (sockaddr*)&addr, sizeof(addr));
    std::cout << "[客户端] 已连接\n";

    // 4. 发送
    const char* msg = "Hello TCP!\n";
    send(sock, msg, strlen(msg), 0);
    std::cout << "[客户端] 已发送: " << msg;

    // 5. 接收回复
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[客户端] 收到回复: " << buf;
    }

    // 6. 关闭
    close(sock);
    return 0;
}
```

编译：`g++ -std=c++17 -o client client.cpp`

---

### 逐段详解

#### 为什么多了 `<arpa/inet.h>`

```cpp
#include <arpa/inet.h>   // inet_pton() — IP 字符串 → 二进制
```

服务端用 `INADDR_ANY`（一个定义为 0 的宏），不需要把字符串转成地址。客户端要指定 "连 127.0.0.1"，得把人类可读的字符串转成 `sockaddr_in` 能用的二进制格式，所以多这个头文件。

---

#### `inet_pton()` — IP 字符串转二进制

```cpp
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
```

| 参数 | 值 | 含义 |
|------|-----|------|
| `AF_INET` | IPv4 | 输出什么格式的地址 |
| `"127.0.0.1"` | 输入字符串 | 人类读的 IP |
| `&addr.sin_addr` | 输出 | 转好的 32 位二进制写进这里 |

**P**resentation → **N**umeric，呈现格式（字符串）→ 数字格式（二进制）。

反向操作 `inet_ntop()`：二进制 → 字符串，用于打印客户端 IP。

---

#### `connect()` — 三次握手在此完成

```cpp
connect(sock, (sockaddr*)&addr, sizeof(addr));
```

**这一行的背后发生了什么：**

```
你的代码调用 connect()
    ↓
内核发送 SYN 包 → 127.0.0.1:9999
    ↓
服务器内核收到 SYN，回复 SYN+ACK
    ↓
你的内核收到 SYN+ACK，自动回复 ACK
    ↓
三次握手完成！connect() 返回 0
    ↓
与此同时，服务器的 accept() 解阻塞，拿到 client_fd
```

`connect()` 和 `accept()` 在时间上精确碰头——connect 返回 0 时，对方的 accept 刚好返回。

**为什么一行 connect 就完成了三次握手：** 网络协议栈在**内核**里。你的代码不需要手动构造 SYN 包——`connect()` 是个系统调用，对内核说"帮我建立到 X 的 TCP 连接"，内核帮你完成构造 SYN、等 SYN+ACK、回 ACK、超时重传等所有事。

**返回值：**

| 返回值 | 含义 |
|--------|------|
| `0` | 连接成功 |
| `-1` | 失败：服务器没开 / 端口错 / 防火墙拦截 / 网络不通 |

**常见错误和原因：**

| 错误提示 | 原因 |
|---------|------|
| `Connection refused` | 服务器没启动或端口号不对 |
| `Connection timed out` | 防火墙拦截 |
| `Network is unreachable` | 网络不通 |

---

#### 客户端只有一个 socket

```cpp
int sock = socket(AF_INET, SOCK_STREAM, 0);
// ...
send(sock, msg, strlen(msg), 0);
recv(sock, buf, sizeof(buf) - 1, 0);
close(sock);
```

客户端 `sock` 同时用于发送和接收，因为 TCP 是全双工的——双向都可以收发。不像服务端分 server_fd（只监听）和 client_fd（只通信）。

---

#### `strlen` vs `sizeof` 的坑

```cpp
const char* msg = "Hello TCP!\n";
send(sock, msg, strlen(msg), 0);   // strlen = 11（正确）
// send(sock, msg, sizeof(msg), 0); // sizeof = 8（错误！指针本身的大小）
```

`"Hello TCP!\n"` 是 11 个字符加 1 个 `\0` = 12 字节。但 `msg` 是指针，`sizeof(msg)` 在 64 位系统永远是 8（指针大小），不是字符串长度。**发送字符串时用 `strlen`，不要用 `sizeof`。**

---

## 三、完整调用时序图

```
服务器端                           客户端
────────                          ────────
socket()                          socket()
  │                                 │
bind() ← 登记 IP:Port              │
  │                                 │
listen() ← 转为 LISTEN 状态        │
  │                                 │
  │    ←── SYN ──────────────── connect() ← 发起三次握手
  │    ── SYN+ACK ───────────→      │         （内核自动完成）
  │    ←── ACK ────────────────     │
  │                                 │
accept() ← 解阻塞，拿到 client_fd   │
  │                                 │
  │    ←── "Hello TCP!\n" ──── send()
recv() ← 收到                       │
  │                                 │
send() ── "Hello TCP!\n" ──→   recv() ← 收到回复
  │                                 │
close(client_fd) ← 四次挥手 →  close() ← 客户端进 TIME_WAIT
close(server_fd)
```

---

## 四、任务清单

### 任务 1：编译运行

**终端 1：**
```bash
cd practical/demo
./server
```

**终端 2：**
```bash
cd practical/demo
./client
```

记录两个终端的输出。

---

### 任务 2：strace 追踪系统调用

**终端 1** 先起 `./server`，**终端 2** 执行：
```bash
strace -e trace=network ./client 2>&1
```

预期看到：
```
socket(AF_INET, SOCK_STREAM, ...) = 3
connect(3, ...) = 0                   ← 三次握手
sendto(3, "Hello TCP!\n", 11, ...)    ← 发送 11 字节
recvfrom(3, "Hello TCP!\n", 1023, ...) = 11  ← 接收 11 字节
```

---

### 任务 3：ss 验证 TIME_WAIT

**终端 1** 起 `./server`，**终端 2** 执行：
```bash
# 跑完客户端后立刻查
./client && ss -tnp | grep 9999
```

你应该看到一条 `TIME-WAIT` 状态的连接。**谁主动 close，谁进 TIME_WAIT。** 客户端先 `close(sock)` 所以是客户端进。

等一分钟后重查：`ss -tnp | grep 9999`，TIME_WAIT 消失了。

---

### 任务 4：nc 手动模拟 TCP 聊天

**终端 1：** `nc -l 9999`
**终端 2：** `nc 127.0.0.1 9999`
**终端 3（可选）：** `ss -tnp | grep 9999` 看 ESTAB 状态

在终端 1 打字回车 → 终端 2 能看到；反过来也可以。nc 屏蔽了 socket 细节，但底层做的事和你写的 server/client 一样。

---

## 五、自测题

1. 服务器 8 步 `socket → setsockopt → bind → listen → accept → recv → send → close` 每步的职责各一句话
2. `server_fd` 和 `client_fd` 有什么区别？为什么服务端需要两个，客户端只需要一个？
3. `connect()` 这一个系统调用 = TCP 的什么过程？为什么一行代码背后是三次握手？
4. `htons()` 不调用行不行？为什么？
5. `recv()` 返回 0 代表什么？你的代码有处理吗？
6. 客户端 `close()` 后，哪一方进入 TIME_WAIT？为什么不是另一方？

<details>
<summary>点击查看答案</summary>

1. socket=创建电话机、setsockopt=允许端口复用、bind=插上电话线登记号码、listen=开始等电话、accept=接电话(阻塞)、recv=听对方说话、send=回话、close=挂断触发四次挥手
2. server_fd 只用来接新连接(accept)，一个程序只有一个；client_fd 用来和具体客户端通信(send/recv)，每 accept 一次产生一个。客户端只需要 sock 这一个 fd，因为它是主动发起方，连上对方后直接用它收发
3. connect() 向服务器发起连接，内核自动完成 SYN→SYN+ACK→ACK 三次握手后才返回。一行代码=三次握手是因为协议栈在内核里
4. 不行。x86 是小端，网络是大端。不转 htons(9999) 对方按网络序解读成 3879，端口对不上 connect 失败
5. recv() 返回 0 = 对方已调用 close()，连接关闭(FIN 已收到)。当前代码只处理了 n>0 的情况，没有处理 n=0 和 n<0
6. 客户端进 TIME_WAIT。因为客户端先 close(sock)，主动关闭方进 TIME_WAIT，持续约 60 秒

</details>

---

> 所有函数的完整参数表、返回值、底层原理见 [Socket API 函数速查手册](socket-api-reference.md)