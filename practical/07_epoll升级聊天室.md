# 第七课：epoll 升级聊天室 —— 从 select 到 epoll

> 把第五课的聊天室从 select 升级到 epoll。核心新知识：`epoll_create1` + `epoll_ctl` + `epoll_wait` 三兄弟。

---

## 一、为什么需要 epoll

select 有三个痛点，你在前两课应该已经有感觉了：

### 痛点 1：每次循环都要重建 bitmap

```cpp
while (true) {
    FD_ZERO(&read_fds);           // 清零
    FD_SET(server_fd, &read_fds); // 重加
    for (int fd : clients) {      // 遍历重加
        FD_SET(fd, &read_fds);
    }
    select(...);
    // select 返回后 bitmap 被修改，下轮不重建就漏fd
}
```

### 痛点 2：内核也要 O(n) 扫描整个 bitmap

```
select(max_fd+1, ...) → 内核从 fd=0 扫到 fd=max_fd
1024 个 fd 只有 5 个在用，剩下 1019 个白扫
```

### 痛点 3：fd_set 上限 1024

`fd_set` 底层是固定 1024 位的 bitmap，fd 编号超过 1023 无法监控。

### epoll 怎么解决

| select 问题 | epoll 方案 |
|-------------|-----------|
| 每次重建 bitmap | `epoll_ctl` **一次性注册**，永久有效 |
| 内核 O(n) 扫描 | `epoll_wait` 只返回**有事件的 fd**，O(1) |
| fd 上限 1024 | 无上限，百万连接也不怕 |

**核心思想：** select 是"你每次告诉内核盯谁"，epoll 是"内核帮你记着，有事叫你就行"。

```
select 模式：                     epoll 模式：
每轮重说"盯fd3,4,5"              注册一次"盯fd3,4,5"
内核扫一遍全部fd                    内核在背后记着
    ↓                                 ↓
下轮又重说…                         "fd4有动静了" ← 只通知这一个
```

---

## 二、epoll 三兄弟

```cpp
#include <sys/epoll.h>

// ① 创建 epoll 实例 —— 造一个"事件管家"
int epoll_fd = epoll_create1(0);

// ② 注册/修改/删除要盯的 fd —— "管家，帮我盯着这个fd"
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);

// ③ 等待事件发生 —— "管家，有动静了叫我"
int n = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);
```

---

## 三、API 详解

### 3.1 `epoll_create1(0)` —— 创建 epoll 实例

```cpp
int epoll_fd = epoll_create1(0);
```

在内核里创建一个 epoll 实例，返回一个 **fd**。是的——epoll 实例本身也是一个文件描述符。结束时 `close(epoll_fd)` 释放。

参数 `0` 是标志位，没有特殊需求就传 0。

### 3.2 `epoll_ctl` —— 注册/修改/删除

```cpp
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

| 参数 | 含义 |
|------|------|
| `epfd` | epoll 实例的 fd |
| `op` | 操作：`EPOLL_CTL_ADD` / `EPOLL_CTL_MOD` / `EPOLL_CTL_DEL` |
| `fd` | 要监控的目标 fd |
| `event` | 事件描述结构体 |

**三个操作：**

| op | 含义 | 什么时候用 |
|----|------|-----------|
| `EPOLL_CTL_ADD` | 新增监控 | 新连接 accept 后 / server_fd 初始化 |
| `EPOLL_CTL_MOD` | 修改监控方式 | 比如从"只读"改成"读写都监" |
| `EPOLL_CTL_DEL` | 删除监控 | 客户端断开时（必须删，不删 epoll 下次还报） |

**事件结构体：**

```cpp
struct epoll_event ev;
ev.events = EPOLLIN;     // 监控"可读"事件
ev.data.fd = server_fd;  // 事件发生时，告诉我哪个 fd
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `events` | `uint32_t` | 监控什么：`EPOLLIN`（可读）、`EPOLLOUT`（可写）、`EPOLLET`（边缘触发） |
| `data.fd` | `int` | 这个事件属于哪个 fd |

### 3.3 `epoll_wait` —— 等待事件

```cpp
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
```

| 参数 | 含义 |
|------|------|
| `epfd` | epoll 实例 |
| `events` | **出参数组**，内核把有事件的 fd 填进去 |
| `maxevents` | 数组容量，最多一次返回多少个事件 |
| `timeout` | -1 = 一直等；0 = 立刻返回不阻塞；>0 = 等 N 毫秒 |

**返回值 `n`**：有动静的 fd 数量。`events[0]` 到 `events[n-1]` 每个都是真正有数据的 fd。

### 3.4 和 select 的核心区别

```cpp
// select：遍历全部clients，逐个问"是你吗？"    → O(n)
for (auto it = clients.begin(); it != clients.end(); ) {
    int fd = *it;
    if (FD_ISSET(fd, &read_fds)) { ... }   // 逐个排查
    ++it;
}

// epoll：遍历events数组，里面全是"是我是我"  → O(1)
for (int i = 0; i < n; ++i) {
    int fd = events[i].data.fd;             // 直接取
    ...
}
```

假设 1000 个客户端，只有 2 个发了消息：
- select：遍历 1000 次，FD_ISSET 查 1000 次，998 次白查
- epoll：遍历 2 次，0 次白查

---

## 四、完整代码

```cpp
#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>    // 🆕 epoll 三兄弟

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "[服务室服务器] 监听 0.0.0.0:9999\n";

    std::vector<int> clients;  // 存储所有已连接客户端

    // ===== epoll 初始化（只做一次）=====
    int epoll_fd = epoll_create1(0);                   // ① 创建管家

    struct epoll_event ev;                             // ② 准备注册
    ev.events = EPOLLIN;                               //   监控"可读"
    ev.data.fd = server_fd;                            //   属于 server_fd
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev); // ③ 注册

    struct epoll_event events[1024];                   // ④ 接收数组

    while (true) {
        int n = epoll_wait(epoll_fd, events, 1024, -1); // = select()

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {                     // 新连接
                int client_fd = accept(server_fd, nullptr, nullptr);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                clients.push_back(client_fd);
                std::cout << "[聊天室] 新用户 fd=" << client_fd << "\n";
            }
            else {
                char buf[1024];                        // 收消息
                int m = recv(fd, buf, sizeof(buf), 0);
                if (m <= 0) {
                    // 断开
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);  // 取消监控
                    close(fd);
                    for (auto it = clients.begin(); it != clients.end(); ++it) {
                        if (*it == fd) { clients.erase(it); break; }
                    }
                    std::cout << "[聊天室] 离开 fd=" << fd << "\n";
                }
                else {
                    buf[m] = '\0';                     // 广播
                    for (int other : clients) {
                        if (other != fd) {
                            send(other, buf, m, 0);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
```

编译：`g++ -std=c++17 -o chat_server chat_server.cpp`

---

## 五、逐段对比 select vs epoll

### 头文件

```cpp
// select版
#include <sys/select.h>   // 或者不写（socket.h间接包含）

// epoll版
#include <sys/epoll.h>    // 🆕
```

### 初始化

```cpp
// select版：不额外初始化，while内每轮FD_ZERO+FD_SET

// epoll版：while外一次性初始化
int epoll_fd = epoll_create1(0);
struct epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = server_fd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
struct epoll_event events[1024];
```

### while 循环内

| 步骤 | select 版 | epoll 版 |
|------|-----------|----------|
| 等事件 | `select(max_fd+1, &read_fds, NULL, NULL, NULL)` | `int n = epoll_wait(epoll_fd, events, 1024, -1)` |
| 拿结果 | 遍历 clients → `FD_ISSET(fd, &read_fds)` 逐个查 | 遍历 `events[0..n-1]`，全是有事件的 |
| 新连接 | `if (FD_ISSET(server_fd, &read_fds))` | `if (fd == server_fd)` |
| 注册客户端 | 不需要（下次 FD_SET 自动加入） | `epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev)` |
| 断连 | `close(fd)` + `erase` | `epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL)` + `close(fd)` + `erase` |

### 为什么断连必须 DEL

select 是每轮重建 bitmap——不在 vector 里的 fd 自然不会出现在本轮监控里。

epoll 是注册一次永久有效。fd 已经 `close()` 了你还不 `EPOLL_CTL_DEL`，epoll 下次还会报告这个 fd 有动静，但 `recv` 时发现 fd 无效就出错了。

**`epoll_ctl(DEL)` 第三个参数可以传 NULL**——DEL 操作不需要事件描述。

---

## 六、完整流程对比图

### select 版流程

```
while(true) {
    FD_ZERO + FD_SET(server) + FD_SET(所有client)  ← 每轮重建
        ↓
    select()   ← 内核扫描fd 0~max_fd
        ↓
    FD_ISSET(server) → accept
        ↓
    for(遍历全部clients) {
        FD_ISSET(client) → 逐个排查 ← O(n)
        └─ 处理消息/断连
    }
}
```

### epoll 版流程

```
启动时：epoll_create1 + epoll_ctl(ADD server)  ← 只做一次
─────────────────────────────────────────────
while(true) {
    epoll_wait()   ← 内核只返回有事件的fd
        ↓
    for(只遍历有事件的fd) {          ← O(1)
        fd==server → accept + epoll_ctl(ADD)
        fd==client → recv → 处理消息/断连(+ epoll_ctl DEL)
    }
}
```

---

## 七、完整事件生命周期

```
1. 客户端连接：
   TCP SYN → epoll_wait 返回 server_fd
          → accept 拿到 client_fd
          → epoll_ctl(ADD, client_fd)    ← 注册进epoll
          → clients.push_back(client_fd)  ← 记进花名册

2. 客户端发消息：
   数据到达 → epoll_wait 返回 client_fd
           → recv → 广播给 clients 里其他人

3. 客户端断开：
   TCP FIN → epoll_wait 返回 client_fd
           → recv 返回 0
           → epoll_ctl(DEL, client_fd)    ← 从epoll移除
           → close(client_fd)             ← 关socket
           → clients.erase(client_fd)     ← 从花名册移除
```

---

## 八、select vs epoll 最终对比总览

| | select | epoll |
|------|--------|-------|
| 头文件 | `<sys/select.h>` | `<sys/epoll.h>` |
| 初始化 | 无（每轮重建） | `epoll_create1` + `epoll_ctl(ADD)`（一次） |
| 等事件 | `select(max_fd+1, ...)` | `epoll_wait(epoll_fd, ...)` |
| 拿结果 | O(n) 遍历全部 + FE_ISSET | O(1) 只遍历有事件的 |
| 注册新fd | 自动（下轮 FD_SET） | 显式 `epoll_ctl(ADD)` |
| 删除fd | 自动（不在vector就不SET） | 显式 `epoll_ctl(DEL)` |
| fd 上限 | 1024 | 无限制 |
| 内核扫描 | 每次 O(n) | 回调 O(1) |
| 内存开销 | 1024 位 bitmap (~128B) | epoll 实例 + 红黑树 |

---

## 九、自测题

1. epoll 三个函数分别干什么？各自什么时候调用？
2. 为什么 epoll 不需要每轮循环重建监控列表？
3. `epoll_wait` 返回的 `events` 数组和 `select` 返回的 `fd_set` 有什么区别？
4. 为什么 epoll 里新连接 accept 后要调用 `epoll_ctl(ADD)`？
5. 为什么断连时要调用 `epoll_ctl(DEL)`？select 里为什么不需要？
6. 1000 个客户端只有 2 个发了消息，select 和 epoll 分别要检查多少个 fd？

<details>
<summary>点击查看答案</summary>

1. `epoll_create1(0)`：创建 epoll 实例，程序启动时调用一次。`epoll_ctl(ADD/MOD/DEL)`：注册/修改/删除被监控的 fd，新连接时 ADD、断连时 DEL。`epoll_wait`：等待事件，while 循环里每轮调用替代 select。

2. epoll 在内核里维护了被监控 fd 的列表（红黑树）。`epoll_ctl(ADD)` 是往这个列表里加项，一次添加永久有效（直到 DEL）。select 没有这个持久化存储——每次都要把 bitmap 传给内核，内核用完就扔。

3. `fd_set` 返回时保留了所有被监控的 fd，但只有有动静的 fd 的 bit 还亮着——你必须遍历全部 fd 用 `FD_ISSET` 逐个查谁还亮着。`events` 数组返回的**只有**有动静的 fd——`events[0]` 到 `events[n-1]` 全是干货，直接取即可。

4. select 每轮重建 bitmap 时，vector 里的 fd 自动被 `FD_SET` 纳入监控。epoll 是注册一次永久有效——新 fd 不显式 `epoll_ctl(ADD)` 就永远不会被监控。

5. 不 DEL 的话 epoll 下次还会报告这个已关闭的 fd 有事件，但 `recv` 时你会发现 fd 已经无效了。select 不需要是因为它每轮重建 bitmap——已关闭的 fd 自然不在 vector 里，就不会被 FD_SET。

6. select：遍历检查 1000 个 client_fd + 1 个 server_fd = 1001 次 FD_ISSET。epoll：只遍历 2 个 events（或者加上 server_fd 共 3 个）。

</details>