# Socket API 函数速查手册

> 本手册按调用顺序排列，附带底层原理讲解。

---

## 一、服务器端七步

### 1. `socket()` — 创建 socket

```cpp
int socket(int domain, int type, int protocol);
```

**作用：** 向内核申请创建一个 socket，返回文件描述符（整数）。失败返回 -1。

**参数：**

| 参数 | 常用值 | 含义 |
|------|--------|------|
| `domain` | `AF_INET`（IPv4）、`AF_INET6`（IPv6） | 地址族，用什么版本的 IP |
| `type` | `SOCK_STREAM`（TCP）、`SOCK_DGRAM`（UDP） | 传输层协议类型 |
| `protocol` | `0` | 前两个参数已经确定协议，填 0 自动选 |

**返回值：** 文件描述符（非负整数）。Linux 上 socket 就是一个 fd，规则和文件一样——最小的未使用编号。

**类比：** 去营业厅办电话卡，得到一个号码。

---

### 2. `setsockopt()` — 设置 socket 选项

```cpp
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
```

**为什么需要：** 服务器关闭后端口会处于 `TIME_WAIT` 状态约 1~2 分钟，立刻重启会报 "Address already in use"。`SO_REUSEADDR` 允许复用这一状态的端口。

**参数：**

| 参数 | 本例取值 | 含义 |
|------|---------|------|
| `sockfd` | `server_fd` | 要设置的 socket |
| `level` | `SOL_SOCKET` | 在 socket 层设置（不是 TCP 层） |
| `optname` | `SO_REUSEADDR` | 允许地址复用 |
| `optval` | `&opt`（=1） | 1=开启，0=关闭 |
| `optlen` | `sizeof(opt)` | 值的大小 |

---

### 3. `bind()` — 绑定地址和端口

```cpp
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

**作用：** 把 socket 绑定到具体的 IP 地址和端口号。绑定后这个端口归你，别人不能再占。

**类比：** 把电话机插到墙上的电话插孔上。在这之前电话机是孤立的，没接入电话网。

**参数：**

| 参数 | 含义 |
|------|------|
| `sockfd` | 绑哪个 socket |
| `addr` | 地址信息（`sockaddr_in` 强转成 `sockaddr*`） |
| `addrlen` | 结构体大小 |

---

### `sockaddr_in` 结构体 — 地址的载体

```cpp
// 定义在 <netinet/in.h>
struct sockaddr_in {
    sa_family_t    sin_family;   // 地址族（2 字节）
    in_port_t      sin_port;     // 端口号，网络字节序（2 字节）
    struct in_addr sin_addr;     // IP 地址（4 字节）
    char           sin_zero[8];  // 填充字段，永远全零（8 字节）
};  // 总共 16 字节
```

**字段逐个解释：**

| 字段 | 类型 | 大小 | 含义 | 填什么 |
|------|------|------|------|--------|
| `sin_family` | `sa_family_t`（= `unsigned short`） | 2B | 地址族 | `AF_INET` = IPv4, `AF_INET6` = IPv6 |
| `sin_port` | `in_port_t`（= `uint16_t`） | 2B | 端口号 | `htons(端口号)`，必须转网络字节序 |
| `sin_addr` | `struct in_addr` | 4B | IP 地址 | `INADDR_ANY` 或 `inet_pton(...)` |
| `sin_zero` | `char[8]` | 8B | 占位填充 | 永远全零，让结构体大小和通用 `sockaddr` 一致 |

#### 内部的 `struct in_addr`

```cpp
struct in_addr {
    in_addr_t s_addr;   // 32 位无符号整数（uint32_t），存 IP，网络字节序
};
```

**就一个字段 `s_addr`。** 当初设计时想抽象成结构体方便扩展，但从未扩展过。所以访问路径是两层点：

```cpp
addr.sin_addr.s_addr = INADDR_ANY;
//       ↑        ↑
//    in_addr   in_addr_t
```

`sin_addr` 是 `in_addr` 类型，`s_addr` 是里面的 `uint32_t`。

#### 三种填 IP 地址的方式

| 方式 | 代码 | 适合场景 |
|------|------|---------|
| 监听所有网卡 | `addr.sin_addr.s_addr = INADDR_ANY;` | 服务器，接收所有来源 |
| 字符串转二进制 | `inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);` | 客户端，**推荐**，自动处理字节序 |
| 老式转换（废弃） | `addr.sin_addr.s_addr = inet_addr("127.0.0.1");` | 不要用，`inet_pton` 是替代 |

#### 为什么需要 `sin_zero[8]`

为了和通用 `sockaddr` 结构体大小一致。`sockaddr` 是 16 字节，`sockaddr_in` 字段加起来 8 字节，剩下的 8 字节用 `sin_zero` 补齐。代码里**永远不需要手动填**——`{}` 零初始化会自动清零。

```cpp
sockaddr_in addr{};   // {} 把 sin_zero 也初始化为全零
```

---

### 字节序详解

#### 为什么有字节序问题

多字节整数在内存里的存放方式，不同 CPU 架构不一样：

```
十进制 9999 = 十六进制 0x270F = 二进制 00100111 00001111

大端（Big-Endian = 网络字节序）：高位字节存低地址
  地址:  [0]    [1]
  内容:  0x27   0x0F     ← 高位在前，符合人类阅读习惯

小端（Little-Endian = x86/你的电脑）：低位字节存低地址
  地址:  [0]    [1]
  内容:  0x0F   0x27     ← 低位在前，x86 CPU 原生格式
```

**网络协议统一规定用大端（网络字节序）。** 你的 CPU 是小端，不转换的话：

```cpp
// 不转换的错误示例
addr.sin_port = 9999;   // CPU 存成 0x0F27（小端）
                        // 网络读成 0x0F27 = 3879 ≠ 9999
                        // 端口对不上，connect 失败！
```

#### 四个转换函数

```cpp
#include <arpa/inet.h>   // 需要这个头文件

// h = Host（主机字节序）, n = Network（网络字节序）
// s = Short（16位）, l = Long（32位）

uint16_t htons(uint16_t hostshort);    // Host TO Network Short  — 端口号专用
uint32_t htonl(uint32_t hostlong);     // Host TO Network Long   — IP 地址用
uint16_t ntohs(uint16_t netshort);     // Network TO Host Short  — 反向转换
uint32_t ntohl(uint32_t netlong);      // Network TO Host Long   — 反向转换
```

| 函数 | 方向 | 位数 | 用在 |
|------|------|------|------|
| `htons()` | 主机→网络 | 16 位 | **端口号**（sin_port） |
| `htonl()` | 主机→网络 | 32 位 | IP 地址（sin_addr.s_addr） |
| `ntohs()` | 网络→主机 | 16 位 | 打印对方端口时转回来 |
| `ntohl()` | 网络→主机 | 32 位 | 打印对方 IP 时转回来 |

**底层就一条 CPU 指令**（`bswap`）：把两个字节或四个字节颠倒顺序。

#### `inet_pton()` 内部已经做了字节序转换

```cpp
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
// 三步合一：解析字符串 → 组装 32 位 → 转为网络字节序
// 不需要再手动调 htonl()
```

等价于：

```cpp
addr.sin_addr.s_addr = htonl( ((127 << 24) | (0 << 16) | (0 << 8) | 1) );
//                         手动拼 32 位 + 转字节序 ← 麻烦且易出错
```

**结论：填 IP 一律用 `inet_pton`，填端口一律用 `htons`。**

---

### 4. `listen()` — 开始监听

```cpp
int listen(int sockfd, int backlog);
```

**作用：** 告诉内核这个 socket 开始接受连接请求。调用后 socket 从 `CLOSED` 转为 `LISTEN` 状态。

**`backlog` 到底是什么：** 不是"最多几个客户端"，而是**已完成三次握手但还没被 `accept()` 取走的连接的最大排队数量**。超出后新的 SYN 会被拒绝。

**类比：** 电话机进入"会响铃"的状态。有人打进来会响，但你不一定立刻接。

---

### 5. `accept()` — 接受连接

```cpp
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

**作用：** 从等待队列中取出一个已完成的连接。**没有连接时会阻塞**（程序停在这里等）。

**`server_fd` vs `client_fd`：**

```
server_fd = 监听 socket，只用来接新连接（accept），整个程序只有一个
client_fd = 通信 socket，和具体某个客户端收发数据（send/recv），每个客户端一个
```

就像公司前台座机号（server_fd）用来接新客户来电，接到后转分机（client_fd），实际对话在分机上进行。

**参数：**

| 参数 | 含义 |
|------|------|
| `sockfd` | 监听 socket |
| `addr` | **输出参数**，内核把客户端的 IP 和端口填进去 |
| `addrlen` | **输入输出参数**，传结构体大小，内核写实际大小 |

---

### 6. `recv()` — 接收数据

```cpp
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

**作用：** 从连接上读数据。没有数据时**阻塞等待**。

**返回值：**

| 返回值 | 含义 |
|--------|------|
| `> 0` | 实际读到的字节数 |
| `= 0` | 对方已关闭连接（FIN 已收到） |
| `< 0` | 出错 |

**参数 flags：** 一般填 `0`。`MSG_DONTWAIT` 表示非阻塞，`MSG_PEEK` 表示偷看但不取走。

---

### 7. `send()` — 发送数据

```cpp
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

**作用：** 往连接上写数据。TCP 保证对方 `recv()` 能收到（内核帮你做了 ACK/重传/排序）。

**注意：** `send()` 返回只表示数据交给了内核发送缓冲区，**不代表对方已经收到**。真正送达由 TCP 协议栈保证。

---

### 8. `close()` — 关闭连接

```cpp
int close(int fd);
```

**作用：** 关闭文件描述符。对于 TCP socket，**触发四次挥手**（FIN → ACK → FIN → ACK），内核自动完成。

---

## 二、客户端专属

### `inet_pton()` — IP 字符串 → 二进制

```cpp
int inet_pton(int af, const char *src, void *dst);
```

**作用：** 把 `"127.0.0.1"` 这种人类可读的 IP 转成 `sockaddr_in` 能用的 32 位二进制。

| 参数 | 含义 |
|------|------|
| `af` | `AF_INET`（IPv4）或 `AF_INET6`（IPv6） |
| `src` | 人类读的 IP 字符串 |
| `dst` | 输出：二进制结果存这里 |

**反过来：** `inet_ntop()` — 二进制 IP → 字符串。

---

### `connect()` — 发起连接（三次握手）

```cpp
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

**作用：** 向服务器发起 TCP 连接。这个系统调用一执行，内核自动完成三次握手：

```
客户端 connect() →
  内核发 SYN →
  收到 SYN+ACK，内核自动回 ACK →
  三次握手完成，connect() 返回 0
```

**返回值：** `0` = 成功，`-1` = 失败（服务器没开/端口错/防火墙拦截）。

**和服务器 accept() 的关系：** `connect()` 返回时，服务器的 `accept()` 刚好解阻塞，二者在时间上碰头。

---

## 三、完整调用时序

```
服务器                         客户端
──────                        ──────
socket()                      socket()
  │                              │
bind()                           │
  │                              │
listen()                         │
  │                              │
  │    ←── SYN ────────────── connect()  ← 三次握手
  │    ── SYN+ACK ──────────→    │
  │    ←── ACK ──────────────    │
  │                              │
accept() ← 解阻塞               │
  │                              │
  │    ←── "Hello TCP!" ──── send()
recv()                            │
  │                              │
send() ── "Hello TCP!" ────→    recv()
  │                              │
close() ── FIN/ACK ────────→  close()  ← 四次挥手
```

---

## 三、UDP 专属函数

UDP 和 TCP 使用完全相同的 `socket()` + `bind()` + `close()`。区别在于收发——UDP 无连接，每次收发都要指定对方的地址。

### `sendto()` — UDP 发送

```cpp
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
```

**参数：**

| 参数 | 含义 |
|------|------|
| `sockfd` | 用哪个 socket 发 |
| `buf` | 发什么数据 |
| `len` | 发多少字节 |
| `flags` | 一般填 `0` |
| `dest_addr` | **发给谁**（对方的 `sockaddr_in` 强转） |
| `addrlen` | 地址结构体大小 |

**和 TCP `send()` 的区别：**

```
TCP:  send(sock, buf, len, 0);           ← 不需要目标地址，连接已绑定对方
UDP: sendto(sock, buf, len, 0, &addr, len); ← 每次都要指定"发给谁"
```

**`sendto()` 一定成功吗：** UDP 无连接，`sendto()` 只把数据丢到内核发送队列就返回，**不管对方收没收到，也不管端口有没有人监听**。所以你的 UDP 客户端就算服务器没开，`sendto()` 也照常成功。

---

### `recvfrom()` — UDP 接收

```cpp
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
```

**参数：**

| 参数 | 含义 |
|------|------|
| `sockfd` | 从哪个 socket 收 |
| `buf` | 数据读到哪 |
| `len` | 最多读多少字节 |
| `flags` | 一般填 `0` |
| `src_addr` | **输出**：内核把"谁发来的"填进去 |
| `addrlen` | **输入+输出**：传入结构体大小，内核写实际大小 |

**和 TCP `recv()` 的区别：**

```
TCP:  recv(sock, buf, len, 0);                 ← fd 已绑定对方，直接读
UDP: recvfrom(sock, buf, len, 0, &addr, &len);  ← 每次都要问"谁发的"
```

**类比：**

```
TCP recv = 接通的电话，听筒拿起来，声音肯定是对方说的

UDP recvfrom = 坐在传达室，谁都能扔纸条进来
              每张纸条都得看落款才知道谁写的
```

**后两个参数可为 `nullptr`：** 客户端知道就是服务器回的，不关���来源，填 `nullptr` 即可。服务器需要知道客户端地址才能回送，所以不能填 `nullptr`。

---

### UDP 服务器 vs TCP 服务器

```
TCP 服务器（完整流程）：               UDP 服务器（简化流程）：
socket()                              socket()
  │                                     │
bind()                                bind()
  │                                     │
listen()  ← 多这步！                   [没有 listen/accept]
  │                                     │
accept()  ← 多这步！                  recvfrom() ← 直接等数据
  │                                     │
recv()/send() 用 client_fd            sendto()    ← 用记录的客户端地址回送
  │                                     │
close(client_fd) + close(server_fd)    close()
```

**UDP 服务端不需要 `listen()` 和 `accept()`——** 没连接这个概念，谁往这个端口发数据，`recvfrom()` 就能收，同时告诉你谁发的。

---

### TCP vs UDP 函数对照总表

| 步骤 | TCP | UDP | 差异原因 |
|------|-----|-----|---------|
| 创建 | `socket(AF_INET, SOCK_STREAM, 0)` | `socket(AF_INET, SOCK_DGRAM, 0)` | 第二个参数选协议 |
| 绑定 | `bind()` | `bind()` | 完全相同 |
| 监听 | `listen()` | **无** | UDP 无连接 |
| 接受 | `accept()` | **无** | UDP 无连接 |
| 发送 | `send(sock, ...)` | `sendto(sock, ..., &addr, len)` | UDP 每次指定目标 |
| 接收 | `recv(sock, ...)` | `recvfrom(sock, ..., &addr, &len)` | UDP 每次获取来源 |
| 关闭 | `close()` | `close()` | 相同（UDP 无四次挥手） |

---

## 四、常见错误排查

| 错误 | 原因 | 解决 |
|------|------|------|
| `Address already in use` | 端口还在 TIME_WAIT | 加 `SO_REUSEADDR`，或等 1~2 分钟 |
| `Connection refused` | 服务器没运行或端口不对 | 检查 server 是否启动、端口号一致 |
| `Permission denied` | 端口号 < 1024 需要 root | 换成 1024 以上的端口 |
| `connect: Network is unreachable` | IP 地址不对/网络不通 | 先用 ping 测通断 |