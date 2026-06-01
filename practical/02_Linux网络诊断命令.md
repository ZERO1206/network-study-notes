# Linux 网络诊断命令 —— 指导手册

> 每一条命令都自己敲。括号里是你要思考的问题。

---

## 任务 1：curl 看 HTTP

```bash
curl -v http://www.baidu.com 2>&1 | head -30
```

观察输出，找出：
- `>` 开头的行 —— 这是你发出去的
- `<` 开头的行 —— 这是服务器返回的
- `*` 开头的行 —— 这是 TCP 连接建立过程

**思考：** 请求头里的 `Host`、`User-Agent`、`Accept` 各是什么意思？（提示：翻 00 笔记的 HTTP 章节）

---

## 任务 2：ss 查看 socket

```bash
# 本机所有 TCP 监听端口
ss -tlnp

# 本机所有已建立的 TCP 连接
ss -tnp state established
```

**思考：** `LISTEN` 和 `ESTAB` 分别对应 TCP 的什么状态？

---

## 任务 3：nc 练手

打开两个终端。

终端 1（服务器）：
```bash
nc -l 8888
```

终端 2（客户端）：
```bash
nc 127.0.0.1 8888
```

现在你在终端 1 打字，终端 2 能看到吗？反过来呢？

**这就是最原始的 TCP 聊天。** nc 帮你屏蔽了 socket 编程细节，但在底层它做的事和你刚才写的 server/client 代码一模一样。

---

## 任务 4：ping 和 traceroute

```bash
# 测通不通
ping -c 4 baidu.com

# 看经过多少跳
traceroute baidu.com
```

**思考：**
- ping 的时间（RTT）大概多少毫秒？
- traceroute 里有没有 `* * *` 的行？这代表什么？（提示：翻 ICMP 笔记）

---

## 任务 5：nslookup 查 DNS

```bash
nslookup baidu.com
nslookup github.com
```

**思考：** 百度返回了几个 IP？为什么一个域名可以对应多个 IP？

---

## 任务 6：tcpdump 抓包

```bash
# 抓 baidu 的 HTTP 流量（如果你有非代理的直连）
sudo tcpdump -A port 80 -c 20

# 或者在终端 1 起 nc，终端 2 抓 lo 接口
# 终端 1: nc -l 9999
# 终端 2: sudo tcpdump -i lo port 9999
# 终端 3: echo "hello" | nc 127.0.0.1 9999
```

---

## 理解确认

1. `curl -v` 的输出里，你亲眼看到了 HTTP 请求的什么字段？和你笔记第 5 节里写的格式对比一下。
2. 用 `nc` 聊天时，关掉一个终端，另一个会怎样？（试一下）
3. `traceroute` 每一跳的时间是递增的吗？如果中间某跳时间突然变长说明什么？

---

> 把这些命令变成肌肉记忆。以后实习排查网络问题，90% 就是 `ss` + `curl -v` + `ping` + `tcpdump` 这几个来回用。