# ICMP 协议

---

## ICMP 是什么？

**ICMP（Internet Control Message Protocol，互联网控制报文协议）** 是 IP 层的"信使"。不传数据，只**报告网络状态和错误**。

```
ICMP 装在 IP 数据报里 → 但它属于网络层，和 IP 平级
┌─ IP 头 ──┬─ ICMP 报文 ─┐
```

---

## 常见 ICMP 报文类型

| 类型 | 名称 | 触发场景 |
|------|------|---------|
| 0 | Echo Reply | 回复 Ping |
| 3 | Destination Unreachable | 目标不可达 |
| 5 | Redirect | 有更好的路由，通知你 |
| 8 | Echo Request | Ping 发出的请求 |
| 11 | Time Exceeded | TTL 耗尽（Traceroute 靠它） |

---

## Ping 原理

**Ping = 发 ICMP Echo Request（类型8），等 Echo Reply（类型0）。**

```
你(ping baidu.com)
   │
   ├── ICMP Echo Request (类型8) ──→ 百度
   │
   │←── ICMP Echo Reply (类型0) ─── 百度
   │
  计算往返时间 RTT
```

```
$ ping baidu.com
Reply from 110.242.68.66: time=25ms
Reply from 110.242.68.66: time=24ms
```

**Ping 不通 != 对方挂了**。可能是防火墙屏蔽了 ICMP，正常 TCP/HTTP 服务照样工作。

---

## Traceroute 原理

**巧妙利用 TTL**。TTL 每跳减 1，减到 0 路由器丢弃并回 ICMP Time Exceeded（类型11）。

Traceroute 故意发 TTL=1,2,3... 的包，逐跳触发：

```
TTL=1 → 第1跳路由器 TTL=0 → 丢弃 → 回 ICMP Time Exceeded → 知道第1跳是谁
TTL=2 → 第2跳路由器 TTL=0 → 丢弃 → 回 ICMP Time Exceeded → 知道第2跳是谁
TTL=3 → ...

直到 TTL 够大，到达目的地，回 Echo Reply → 追踪完成
```

```
$ traceroute baidu.com
 1  192.168.1.1    1ms      ← 你家路由器
 2  10.0.0.1       5ms      ← ISP 网关
 3  172.16.0.1     8ms      ← 继续往上...
 4  110.242.68.66  25ms     ← 到达！
```

中间路由器不回应会显示 `* * *`——正常，人家不想理你。

---

## Ping vs Traceroute

```
Ping        → "你在不在？"       靠 Echo Request/Reply（类型8/0）
Traceroute  → "去你那儿经过了谁？" 靠 Time Exceeded（类型11）+ TTL
```

---

## 关键点回顾

- ICMP = 网络层的信使，装在 IP 包里但与 IP 平级
- Ping = Echo Request(8) + Echo Reply(0)
- Traceroute = TTL 递增 + Time Exceeded(11) 逐跳探测
- Ping 不通可能是防火墙屏蔽，不一定对方挂了