# 第四课：tcpdump 抓包 —— 亲眼看到 TCP 握手挥手

> tcpdump 是在网卡层面抓取每一个经过的数据包，能看到 SYN、FIN、ACK 等所有 TCP 细节。

---

## 一、核心选项速查

```bash
sudo tcpdump -i eth0           # -i: 指定网卡（Interface）
sudo tcpdump -i any            # 监听所有网卡
sudo tcpdump -i eth0 tcp       # 只看 TCP
sudo tcpdump -i eth0 port 9999 # 只看 9999 端口
sudo tcpdump -i eth0 -n        # -n: 不解析域名，直接显示 IP（更快）
sudo tcpdump -i eth0 -nn       # -nn: 域名+服务名都不解析，纯数字
sudo tcpdump -i eth0 -c 10     # -c: 抓 10 个包就停（Count）
sudo tcpdump -i eth0 -A        # -A: 以 ASCII 显示包内容（能看 HTTP 明文）
sudo tcpdump -i eth0 -X        # -X: 同时显示 Hex + ASCII
sudo tcpdump -i eth0 -v        # -v/-vv/-vvv: 越来越详细
sudo tcpdump -i eth0 -w file.pcap  # -w: 保存到文件（Wireshark 可打开）
sudo tcpdump -r file.pcap          # -r: 读取 pcap 文件
```

---

## 二、过滤器写法

```bash
# 单个条件
sudo tcpdump port 80           # 源或目的端口是 80
sudo tcpdump host 192.168.1.5  # 源或目的 IP
sudo tcpdump src 192.168.1.5   # 来源 IP
sudo tcpdump dst 192.168.1.5   # 目的 IP
sudo tcpdump tcp               # 只看 TCP
sudo tcpdump udp               # 只看 UDP
sudo tcpdump icmp              # 只看 ICMP（Ping/Traceroute）

# 组合（and / or / not）
sudo tcpdump tcp and port 9999
sudo tcpdump src 192.168.1.5 and dst port 80
sudo tcpdump port 80 or port 443
sudo tcpdump not port 22       # 排除 SSH 流量
```

---

## 三、TCP Flags —— 抓包的核心

tcpdump 用方括号标注每个包的 TCP 标志位：

| Flag | 含义 | 出现在 |
|------|------|--------|
| `[S]` | SYN | 第一次握手："我想连你" |
| `[S.]` | SYN+ACK | 第二次握手："好的，我同意" |
| `[.]` | ACK | 确认收到（纯 ACK，不带 SYN/FIN） |
| `[P]` | PSH（Push） | 有数据，尽快交给应用层 |
| `[P.]` | PSH+ACK | 带数据的确认包 |
| `[F]` | FIN | 断开请求 |
| `[F.]` | FIN+ACK | 断开请求+确认（FIN 通常附 ACK） |
| `[R]` | RST | 连接重置（拒绝或异常） |

**记忆诀窍：点 `.` 表示 ACK。** `[S.]` = SYN+ACK，`[.]` = 纯 ACK，`[F.]` = FIN+ACK。

---

## 四、完整 TCP 连接的 9 个包

假设客户端 127.0.0.1:54321 连服务器 127.0.0.1:9999，发一次数据后断开：

```
# ===== 三次握手（包1~3）=====

包1  127.0.0.1.54321 > 127.0.0.1.9999  Flags [S],  seq 1000000, win 65535, length 0
     客户端 → 服务器：SYN，初始序号 1000000

包2  127.0.0.1.9999  > 127.0.0.1.54321  Flags [S.], seq 5000000, ack 1000001, win 65535, length 0
     服务器 → 客户端：SYN+ACK，确认收到客户端的 SYN（ack=客户端seq+1）

包3  127.0.0.1.54321 > 127.0.0.1.9999  Flags [.],  ack 5000001, win 65535, length 0
     客户端 → 服务器：ACK，确认收到服务器的 SYN
     ← 此时 connect() 和 accept() 返回，连接建立

# ===== 数据传输（包4~5）=====

包4  127.0.0.1.54321 > 127.0.0.1.9999  Flags [P.], seq 1000001:1000012, ack 5000001, length 11
     客户端 → 服务器：PSH+ACK，"Hello TCP!\n"（11 字节）

包5  127.0.0.1.9999  > 127.0.0.1.54321  Flags [.],  ack 1000012, win 65535, length 0
     服务器 → 客户端：ACK，确认收到数据

     ...服务器回传数据（包6~7 类似，略）...

# ===== 四次挥手（包6~9）=====

包6  127.0.0.1.54321 > 127.0.0.1.9999  Flags [F.], seq 1000012, ack 5000001, length 0
     客户端 → 服务器：FIN+ACK，"我说完了"

包7  127.0.0.1.9999  > 127.0.0.1.54321  Flags [.],  ack 1000013, length 0
     服务器 → 客户端：ACK，"知道了"

包8  127.0.0.1.9999  > 127.0.0.1.54321  Flags [F.], seq 5000001, ack 1000013, length 0
     服务器 → 客户端：FIN+ACK，"我也说完了"

包9  127.0.0.1.54321 > 127.0.0.1.9999  Flags [.],  ack 5000002, length 0
     客户端 → 服务器：ACK，"知道了"  ← 客户端进 TIME_WAIT
```

---

## 五、逐字段解读

```
127.0.0.1.54321 > 127.0.0.1.9999: Flags [S], seq 1000000, win 65535, length 0
```

| 字段 | 含义 | 对应理论 |
|------|------|---------|
| `127.0.0.1.54321` | 源地址:端口（客户端临时端口） | 第4课：IP+端口=唯一标识程序 |
| `>` | 方向 | — |
| `127.0.0.1.9999` | 目的地址:端口（服务器监听端口） | 第4课：端口号 |
| `Flags [S]` | TCP 标志位 | 第8课：SYN=握手第一步 |
| `seq 1000000` | TCP 序号（随机初始值） | 第8课：每个字节编号 |
| `ack 1000001` | 期望对方下一个 seq | 第8课：ACK 确认机制 |
| `win 65535` | 接收窗口大小 | 第8课：流量控制 |
| `length 0` | 数据长度 | 握手/挥手包不携带数据 |

**`length 0` 的意义：** 握手和挥手包只交换 TCP 头部信息（标志位+序号），不携带应用数据。`length 11` 才是有数据的包——那 11 字节就是 "Hello TCP!\n"。

---

## 六、如何抓到这些包

### 真实 Linux / 云服务器（完整行为）

```bash
# 终端1：抓 lo 接口上 9999 端口的包
sudo tcpdump -i lo -nn port 9999

# 终端2：先起 server
./server

# 终端3：跑 client
./client

# 终端1 会依次输出 [S] [S.] [.] [P.] [.] [F.] [.] [F.] [.] 9 个包
```

### WSL2 替代方案

WSL2 的 lo 抓不到。用 `strace` 看系统调用等价验证：

```bash
strace -e trace=network ./client 2>&1
# connect() 返回 = 前 3 个包（[S] [S.] [.]）已完成
# sendto() = 数据包发出
# recvfrom() = 回复收到
# close() = 后 4 个包（[F.] [.] [F.] [.]）已触发
```

两者看的是同一件事的不同层面：
```
tcpdump → 网卡上跑的原始报文（包级别）
strace  → 程序调的系统调用（函数级别）
```

有云服务器后用第一种方式亲眼验证即可。

---

## 七、验证 TIME_WAIT

用 `ss` 抓四次挥手后的残留状态——这个无需 tcpdump，在你现有环境就能看：

```bash
# 终端1：./server
# 终端2：
./client && ss -tnp | grep 9999
```

立刻执行，你能看到一条 `TIME-WAIT` 连接——**谁先 close 谁进 TIME_WAIT，持续约 60 秒。** 这就是挥手第 9 个包（客户端最后的 ACK）发出后的状态。

---

## 八、理解确认

1. `[S]`、`[S.]`、`[.]`、`[F.]` 各代表什么 TCP 包？
2. 为什么握手和挥手包的 `length 0`？
3. 四次挥手 4 个包的 Flag 分别是什么？
4. connect() 返回成功时，tcpdump 应该已经抓到了哪几个包？
5. 哪个包之后客户端进入 TIME_WAIT？

> 答案：1-SYN/SYN+ACK/ACK/FIN+ACK  2-只交换控制信息无数据  3-[F.][.][F.][.]  4-前3个[S][S.][.]  5-挥手最后一个[.]