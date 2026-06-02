# 第二课：Linux 网络诊断命令

> 每一条命令都自己敲。知道什么时候该用哪个工具比记住所有参数重要。

---

## 全景速查

| 场景 | 命令 | 一句话 |
|------|------|--------|
| HTTP 接口调不通 | `curl -v` | 看请求发没发、服务器回了啥 |
| 端口被谁占了 | `ss -tlnp` | 查谁在 LISTEN |
| 连接建立了没 | `ss -tnp` | 看 ESTAB / TIME-WAIT |
| 两台机器通不通 | `ping -c 4` | 测 RTT 和丢包率 |
| 经过了多少跳 | `traceroute` | 画路由路径 |
| 域名解析对不对 | `nslookup` | 域名 → IP |
| 终极手段看包内容 | `tcpdump -A` | 抓包看明文 |

---

## 一、curl — HTTP 客户端

### 基本用法

```bash
curl http://www.baidu.com              # 发 GET，打印响应体
curl -v http://www.baidu.com           # -v: verbose，显示请求和响应头
curl -v http://www.baidu.com 2>&1 | head -30  # 只看前面部分
```

### `-v`（verbose，详细模式）输出解读

```
*  开头的行 = curl 自己的日志（TCP 连接建立、TLS 握手、代理等）
>  开头的行 = 客户端发出的 HTTP 请求头
<  开头的行 = 服务器返回的 HTTP 响应头
空行后面的   = 响应体（网页 HTML）
```

你亲眼看到的 `curl -v` 输出对照：

```
* Trying 127.0.0.1:7897...               ← 连代理
* Established connection to 127.0.0.1    ← TCP 连接建立（三次握手完成）
> GET http://www.baidu.com/ HTTP/1.1     ← 请求方法 + 路径 + 版本
> Host: www.baidu.com                    ← 告诉服务器访问哪个网站
> User-Agent: curl/8.18.0                ← 客户端身份
> Accept: */*                            ← 接受任何类型响应
< HTTP/1.1 200 OK                        ← 状态码：成功
< Content-Type: text/html                ← 返回的是 HTML
< Set-Cookie: ...                        ← 服务器让你存 Cookie
```

### 常用选项

| 选项 | 作用 | 示例 |
|------|------|------|
| `-v` | verbose，显示请求头+响应头+连接过程 | `curl -v http://xxx` |
| `-I` | 只取响应头（HEAD 请求），不看正文 | `curl -I http://xxx` |
| `-X POST` | 指定请求方法（GET/POST/PUT/DELETE） | `curl -X POST http://xxx` |
| `-H` | 添加自定义请求头 | `curl -H "Content-Type: application/json"` |
| `-d` | 发送 POST 数据 | `curl -d "name=张三" http://xxx` |
| `-o` | 下载到指定文件 | `curl -o a.jpg http://xxx/img.jpg` |
| `-O` | 下载并用远程文件名保存 | `curl -O http://xxx/img.jpg` |
| `-k` | 忽略 SSL 证书验证（测试用） | `curl -k https://xxx` |
| `-L` | 跟随 301/302 重定向 | `curl -L http://xxx` |
| `-s` | 静默模式，不显示进度条 | `curl -s http://xxx` |

### 实战示例

```bash
# 看一个完整的 HTTP 交互过程
curl -v http://www.baidu.com 2>&1 | head -30

# 测试 POST 接口
curl -v -X POST http://localhost:3000/api \
  -H "Content-Type: application/json" \
  -d '{"name":"张三","age":20}'

# 只看响应头（判断服务器类型、缓存策略）
curl -I http://www.baidu.com

# 跟随重定向（301→新地址）
curl -L http://baidu.com
```

---

## 二、ss — 查看 socket 状态

> `ss` 已替代老旧的 `netstat`。速度快，信息准确。

### 关键参数

| 参数 | 含义 | 记忆 |
|------|------|------|
| `-t` | 只看 **T**CP | t = tcp |
| `-u` | 只看 **U**DP | u = udp |
| `-l` | 只看正在 **L**ISTEN（监听）的 | l = listening |
| `-n` | 不解析域名/服务名，直接显示数字 | n = numeric，速度更快 |
| `-p` | 显示 **P**rocess（哪个进程占用的） | p = process |
| `-a` | 显示 **A**ll（所有状态） | a = all |

### 常用组合

```bash
# 1. 本机所有 TCP 监听端口（最常用）
ss -tlnp
# 输出解读：
# State    Recv-Q  Send-Q  Local Address:Port  Peer Address:Port  Process
# LISTEN   0       3       0.0.0.0:9999        0.0.0.0:*         users:(("server",pid=13272,fd=3))

# LISTEN     = 状态：正在等连接
# 0.0.0.0:9999 = 监听在所有网卡的 9999 端口
# fd=3       = 这个 socket 在进程中对应文件描述符 3

# 2. 所有已建立的 TCP 连接
ss -tnp state established

# 3. 只看某个端口
ss -tlnp | grep 9999      # 谁在监听 9999？
ss -tnp | grep 9999       # 9999 端口上有哪些连接？

# 4. 看本机所有对外连接（排查外连）
ss -tnp state established
```

### 常见状态含义

| 状态 | TCP 阶段 | 含义 |
|------|---------|------|
| `LISTEN` | — | 服务器在等连接（`listen()` 之后） |
| `SYN-SENT` | 握手 | 客户端发了 SYN，在等 SYN+ACK |
| `SYN-RECV` | 握手 | 服务器收到 SYN，回了 SYN+ACK，在等 ACK |
| `ESTAB` | 握手后 | 连接已建立（`connect()`/`accept()` 返回之后） |
| `FIN-WAIT-1` | 挥手 | 主动关闭方发了 FIN |
| `FIN-WAIT-2` | 挥手 | 主动关闭方收到 ACK，在等对方 FIN |
| `CLOSE-WAIT` | 挥手 | 被动关闭方收到 FIN，还没 close |
| `TIME-WAIT` | 挥手后 | 主动关闭方等 2MSL（~60 秒） |

---

## 三、nc（netcat）— TCP/UDP 瑞士军刀

### 基本用法

```bash
# 监听模式（当服务器）
nc -l 9999              # -l: listen, 监听 9999 端口

# 连接模式（当客户端）
nc 127.0.0.1 9999       # 连到 9999 端口

# 发完就关
echo "hello" | nc 127.0.0.1 9999
```

### 常用选项

| 选项 | 作用 |
|------|------|
| `-l` | **L**isten，监听模式 |
| `-p` | 指定本地端口 |
| `-u` | 用 **U**DP 而不是 TCP |
| `-v` | **V**erbose，显示连接详情 |
| `-z` | 只测试端口通不通，不发数据 |
| `-w N` | 超时 N 秒 |

```bash
# 测试端口是否开放（常用！）
nc -zv baidu.com 80
# Connection to baidu.com 80 port [tcp/http] succeeded!

nc -zv 127.0.0.1 9999
# Connection to 127.0.0.1 9999 port [tcp/*] succeeded!
```

---

## 四、ping — 测通断

### 基本用法

```bash
ping baidu.com            # 一直 ping，Ctrl+C 停止看统计
ping -c 4 baidu.com       # -c: Count, 只发 4 个包
ping -i 0.5 baidu.com     # -i: Interval, 每 0.5 秒发一个（默认 1 秒）
```

### 输出解读

```
PING baidu.com (110.242.74.102) 56(84) bytes of data.
64 bytes from 110.242.74.102: icmp_seq=1 ttl=46 time=71.7 ms
64 bytes from 110.242.74.102: icmp_seq=2 ttl=46 time=105 ms
64 bytes from 110.242.74.102: icmp_seq=3 ttl=46 time=85.7 ms
64 bytes from 110.242.74.102: icmp_seq=4 ttl=46 time=72.9 ms

--- baidu.com ping statistics ---
4 packets transmitted, 4 received, 0% packet loss, time 3006ms
rtt min/avg/max/mdev = 71.674/83.938/105.490/13.607 ms
```

| 字段 | 含义 |
|------|------|
| `icmp_seq=1` | 第几个包 |
| `ttl=46` | 还剩余 46 跳。初始通常 64，说明经过了 64-46=**18 跳**到你 |
| `time=71.7 ms` | RTT（往返时间），71.7 毫秒 |
| `0% packet loss` | 丢包率，0% 说明通畅 |
| `min/avg/max/mdev` | 最小/平均/最大/标准差 RTT |

### 关键选项

| 选项 | 作用 |
|------|------|
| `-c N` | 发 N 个包后停止 |
| `-i N` | 间隔 N 秒（默认 1 秒） |
| `-s N` | 包大小（默认 56 字节） |
| `-t N` | 设置 TTL 值 |

### 注意

**ping 不通 != 对方挂了。** 很多服务器防火墙屏蔽 ICMP（禁 ping），但正常 TCP/HTTP 服务照常工作。

---

## 五、traceroute — 看路径

### 基本用法

```bash
traceroute baidu.com
```

### 输出解读

```
traceroute to baidu.com (111.63.65.103), 30 hops max, 60 byte packets
 1  10.36.240.103  9.708 ms  4.845 ms  4.747 ms    ← 第 1 跳，10.x 内网地址
 2  * * *                                           ← 该路由器不回应 ICMP
 3  172.21.4.45    29.069 ms  24.426 ms  37.261 ms   ← 第 3 跳
 ...
11  112.54.105.38  41.754 ms  66.843 ms  65.096 ms   ← 到达百度边缘
12  * * *                                           ← 之后全不回应
```

**每行三个时间**：默认每跳发 3 个探测包，所以三个 RTT。

**`* * *` 的含义：** 该路由器配置了不回应 ICMP Time Exceeded，**不是断了**。公网上非常常见——运营商的安全策略就是不理你。只要最终到达目的地的跳有显示就行。

**为什么第 1 跳就是 10.x.x.x：** 你在大内网里（学校/公司），第一跳就是内网网关。NAT 在外面帮你把地址转了。

### 原理回顾

traceroute 利用 TTL 递增 + ICMP Time Exceeded：
```
发 TTL=1 → 第1跳路由器 TTL=0 → 丢弃 → 回 ICMP Time Exceeded → 知道第1跳
发 TTL=2 → 第2跳路由器 TTL=0 → 丢弃 → 回 ICMP Time Exceeded → 知道第2跳
...
直到 TTL 够大到达目的地 → 目的主机回 ICMP Echo Reply → 结束
```

---

## 六、nslookup — DNS 查询

### 基本用法

```bash
nslookup baidu.com          # 查 A 记录（域名→IPv4）
nslookup github.com         # 返回多个 IP = 多服务器负载均衡
```

### 输出解读

```
Server:         10.255.255.254          ← 当前用的 DNS 服务器（也是 10.x 内网）
Address:        10.255.255.254#53       ← DNS 端口 53

Non-authoritative answer:               ← 非权威回答（缓存转发，不是百度官方 DNS）
Name:   baidu.com
Address: 111.63.65.247                  ← 返回 4 个 IP
Address: 124.237.177.164
Address: 110.242.74.102
Address: 111.63.65.103
```

**为什么一个域名返回多个 IP：** 百度在全国多地部署服务器（CDN）。DNS 返回一堆 IP，浏览器选延迟最低的连。

**Non-authoritative answer：** 你问的是内网 DNS 缓存服务器（10.255.255.254），它不是百度的权威 DNS，是转发/缓存的结果。

### 安装

```bash
sudo apt install bind9-dnsutils -y   # nslookup 和 dig 都在这包里
```

**注意：包名 ≠ 命令名。** `bind9-dnsutils` 是包名，里面包含 `nslookup` 和 `dig` 两个命令。

### 反向查询

```bash
nslookup 110.242.74.102    # IP → 域名（反向解析）
```

---

## 七、tcpdump — 抓包

### 基本用法

```bash
sudo tcpdump -i eth0                    # 抓 eth0 网卡所有流量
sudo tcpdump -i lo                      # 抓本地环回
sudo tcpdump -i eth0 port 80            # 只看 80 端口
sudo tcpdump -i eth0 host baidu.com     # 只看和某主机的通信
sudo tcpdump -i eth0 tcp                # 只看 TCP（不看 UDP/ICMP 等）
sudo tcpdump -i eth0 -c 10              # 抓 10 个包就停
sudo tcpdump -i eth0 -w capture.pcap    # 保存到文件（可用 Wireshark 打开）
sudo tcpdump -r capture.pcap            # 读取 pcap 文件
```

### 关键选项

| 选项 | 作用 |
|------|------|
| `-i eth0` | 指定网卡（**i**nterface） |
| `-A` | 以 **A**SCII 显示包内容（能看到 HTTP 明文！） |
| `-X` | 同时显示 Hex + ASCII |
| `-n` | 不解析域名，直接显示 IP（加速，避免产生额外 DNS 流量） |
| `-nn` | 域名和端口都不解析 |
| `-c N` | 抓 **C**ount 个包后自动停止 |
| `-w file` | **W**rite，保存到 pcap 文件 |
| `-r file` | **R**ead，从 pcap 文件读取 |
| `-v / -vv / -vvv` | 越来越详细的输出 |
| `port N` | 过滤端口 |
| `host X` | 过滤主机 |
| `tcp / udp / icmp` | 过滤协议 |

### 过滤器组合

```bash
# 去往百度 80 端口的 TCP 包
sudo tcpdump -i eth0 tcp and port 80 and host baidu.com

# 来源 192.168.1.5 或 192.168.1.6
sudo tcpdump -i eth0 src 192.168.1.5 or src 192.168.1.6

# 不是 SSH 的流量（排除 22 端口）
sudo tcpdump -i eth0 not port 22
```

### WSL2 限制

WSL2 的虚拟网卡 tcpdump 抓 lo（本地环回）可能抓不到，走代理的流量也抓不到（代理在 Windows 宿主机）。这些不是 tcpdump 的问题，是 WSL2 网络虚拟化的限制。**在有云服务器或原生 Linux 上 tcpdump 完全正常。**

替代方案：用 `strace -e trace=network` 看系统调用（connect/send/recv/close）。

---

## 八、strace — 追踪系统调用

> 不是网络工具，但在实习排查问题时和 tcpdump 互补。

### 基本用法

```bash
strace -e trace=network ./client          # 只看网络相关的系统调用
strace -e trace=network ./client 2>&1     # stderr 重定向到 stdout
```

### 输出解读

```
socket(AF_INET, SOCK_STREAM, IPPROTO_IP) = 3
connect(3, {sa_family=AF_INET, sin_port=htons(9999), sin_addr=inet_addr("127.0.0.1")}, 16) = 0
sendto(3, "Hello TCP!\n", 11, 0, NULL, 0) = 11
recvfrom(3, "Hello TCP!\n", 1023, 0, NULL, NULL) = 11
```

**对照 TCP 理论：**

| strace 输出 | 对应的 TCP 概念 |
|------------|---------------|
| `socket(...) = 3` | 创建 socket，fd=3 |
| `connect(...) = 0` | **三次握手完成**，返回值 0 表示成功 |
| `sendto(3, ..., 11, ...)` | 发送了 11 字节 |
| `recvfrom(3, ..., 1023, ...)` | 接收到回复 |
| 程序退出时 `close(3)` | 触发**四次挥手** |
```

---

## 九、排查问题决策树

```
问题：网页打不开 / API 调不通

① curl -v http://xxx
   ├─ 返回 200？→ 请求正常，问题在别处
   ├─ Connection refused？→ 服务器没启动，或端口不对 → ss -tlnp 查端口
   ├─ Connection timed out？→ 网络不通 → ping 测通断
   ├─ 返回 502/503？→ 服务器内部问题，看服务器日志
   └─ DNS 解析失败？→ nslookup xxx 查 DNS

问题：端口被占用

① ss -tlnp | grep 端口号
   → 看哪个进程占着 → 关掉它或换个端口

问题：连上了但数据不对

① strace 看 send/recv 调用了没
② tcpdump -A 抓包看实际传输了什么
```

---

## 十、任务清单

### 任务 1：curl 看 HTTP 交互
```bash
curl -v http://www.baidu.com 2>&1 | head -30
```
找出 `>`（请求）、`<`（响应）、`*`（连接日志）各一行，写下来。

### 任务 2：ss 查端口
```bash
ss -tlnp                        # 本机所有监听
ss -tnp state established       # 所有已建立的连接
```
然后启动 `nc -l 9999`，另开终端查 `ss -tlnp | grep 9999`，确认能看到 LISTEN。

### 任务 3：nc 双终端聊天
终端 1：`nc -l 9999`，终端 2：`nc 127.0.0.1 9999`。互相打字，看对方能不能收到。

### 任务 4：ping 测通断
```bash
ping -c 4 baidu.com
```
记录 min/avg/max RTT 和丢包率。

### 任务 5：traceroute 看路径
```bash
sudo apt install traceroute -y
traceroute baidu.com
```
数有几跳 `* * *`，想一想为什么。

### 任务 6：nslookup 查 DNS
```bash
sudo apt install bind9-dnsutils -y
nslookup baidu.com
nslookup github.com
```
百度返回了几个 IP？为什么？

### 任务 7：tcpdump 抓本地流量
```bash
# 终端 1：抓包
sudo tcpdump -i lo -A port 9999

# 终端 2：起 nc
nc -l 9999

# 终端 3：发消息
echo "Catch this!" | nc 127.0.0.1 9999
```
WSL2 抓不到的话改用 `strace -e trace=network nc 127.0.0.1 9999` 替代。

---

> 这些命令不需要死记参数，用的时候 `man 命令名` 查。关键是**知道什么场景该用哪个**。