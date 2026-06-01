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
struct sockaddr_in {
    sa_family_t    sin_family;  // 地址族：AF_INET
    in_port_t      sin_port;    // 端口号（网络字节序）
    struct in_addr sin_addr;    // IP 地址
};
```

| 字段 | 填什么 | 含义 |
|------|--------|------|
| `sin_family` | `AF_INET` | IPv4 |
| `sin_addr.s_addr` | `INADDR_ANY` 或 `inet_pton(...)` | `INADDR_ANY`=监听所有网卡 |
| `sin_port` | `htons(9999)` | 端口号，必须转网络字节序 |

---

### `htons()` / `htonl()` — 字节序转换

```cpp
htons(9999);   // Host TO Network Short  (16位: 端口号)
htonl(ip);     // Host TO Network Long   (32位: IP地址)
ntohs(port);   // Network TO Host Short   (反过来)
ntohl(ip);     // Network TO Host Long
```

**为什么要转：** 不同 CPU 存储多字节整数的方式不同：

```
小端（x86/你的电脑）：低位字节在前    9999 = 0F 27
大端（网络标准）：    高位字节在前    9999 = 27 0F
```

网络协议统一规定用**大端（网络字节序）**。不转换的话，你设的 9999 对方可能解析成完全不同端口号。

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

## 四、常见错误排查

| 错误 | 原因 | 解决 |
|------|------|------|
| `Address already in use` | 端口还在 TIME_WAIT | 加 `SO_REUSEADDR`，或等 1~2 分钟 |
| `Connection refused` | 服务器没运行或端口不对 | 检查 server 是否启动、端口号一致 |
| `Permission denied` | 端口号 < 1024 需要 root | 换成 1024 以上的端口 |
| `connect: Network is unreachable` | IP 地址不对/网络不通 | 先用 ping 测通断 |