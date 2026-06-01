# Linux 网络诊断命令 —— 实习必备

---

## 一、curl —— HTTP 客户端

```bash
# 基本 GET 请求
curl http://www.baidu.com

# -v 显示详细信息（请求头+响应头+握手过程）
curl -v http://www.baidu.com
# >  开头 = 客户端发出
# <  开头 = 服务器返回
# *  开头 = curl 日志（TCP 握手等）
```

---

## 二、ss —— 查看 socket 状态（替代 netstat）

```bash
# 查看所有 TCP 监听端口
ss -tlnp

# 查看所有 TCP 连接（含状态）
ss -tnp

# 看某个进程的连接
ss -tnp | grep 9999
# LISTEN     ← 正在监听
# ESTAB      ← 已建立连接
# TIME-WAIT  ← 四次挥手后等待
```

---

## 三、ping —— 测通不通

```bash
ping baidu.com
# Reply from 110.242.68.66: time=25ms

ping -c 4 baidu.com   # 只发 4 个包
```

---

## 四、traceroute —— 看路径

```bash
traceroute baidu.com
# 1 192.168.1.1    1ms    ← 你家路由器
# 2 10.0.0.1       5ms    ← ISP
# 3 ...
# * * *                    ← 该跳不回应，正常
```

---

## 五、nslookup / dig —— DNS 查询

```bash
nslookup baidu.com
# Name:    baidu.com
# Address: 110.242.68.66

dig baidu.com       # 更详细
```

---

## 六、tcpdump —— 抓包

```bash
# 抓 HTTP 流量（能看到明文！）
sudo tcpdump -A port 80

# 抓某个端口的包
sudo tcpdump port 443

# 保存到文件（可用 Wireshark 打开）
sudo tcpdump -w capture.pcap

# 读 pcap 文件
sudo tcpdump -r capture.pcap
```

---

## 七、nc（netcat）—— 瑞士军刀

```bash
# 起一个 TCP 监听
nc -l 9999

# 连接并发送
echo "hello" | nc 127.0.0.1 9999

# 测试端口通不通
nc -zv baidu.com 80
# Connection to baidu.com 80 port [tcp/http] succeeded!
```

---

## 实习常用组合

```bash
# 1. 查某个端口被哪个程序占用
ss -tlnp | grep :8080

# 2. 看本机所有对外连接
ss -tnp state established

# 3. 测 API 接口
curl -v -X POST http://localhost:3000/api -H "Content-Type: application/json" -d '{"key":"value"}'

# 4. 抓包分析 HTTP
sudo tcpdump -A -i eth0 port 80

# 5. 看 DNS 解析对不对
dig +short baidu.com
```