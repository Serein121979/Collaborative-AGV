# 协议说明

## 1. TCP 通信角色

- 手机 -> 1 号车 ESP8266 主站
- 2 号车 ESP8266 从站 -> 1 号车 ESP8266 主站

所有 TCP 消息都使用：

- `ASCII 文本`
- 每条消息以 `\n` 结尾

## 2. 连接注册

### 手机上线

手机发送：

```text
HELLO,PHONE
```

主站返回：

```text
OK,PHONE
```

### 2 号车上线

从站发送：

```text
HELLO,CAR2
```

主站返回：

```text
OK,CAR2
```

## 3. 手机控制命令

格式如下：

```text
CMD,<SEQ>,STOP
CMD,<SEQ>,FWD,<SPEED>
CMD,<SEQ>,BACK,<SPEED>
CMD,<SEQ>,LEFT,<SPEED>
CMD,<SEQ>,RIGHT,<SPEED>
CMD,<SEQ>,SPINL,<SPEED>
CMD,<SEQ>,SPINR,<SPEED>
```

字段说明：

- `<SEQ>`：序号，不能为空，用来回执和排查问题
- `<SPEED>`：速度，省略时默认 `180`

速度规则：

- 有效范围 `0~255`
- 超出范围时，主站会按代码逻辑截断到 `0~255`

## 4. 主站返回给手机的消息

### 合法命令已接收

```text
ACK,<SEQ>
```

### 2 号车不在线

```text
WARN,CAR2_OFFLINE
```

### 2 号车执行完成

```text
STATE,<SEQ>,CAR2_DONE
```

### 2 号车超时未回执

```text
STATE,<SEQ>,CAR2_TIMEOUT
```

### 2 号车中途断开

```text
STATE,<SEQ>,CAR2_DISCONNECTED
```

### 手机发来的命令格式错误

```text
STATE,INVALID_CMD
```

## 5. 主站转发给 2 号车的命令

主站会把高层命令镜像转发给 2 号车，例如：

```text
CMD,<SEQ>,FWD,<SPEED>
```

2 号车执行后返回：

```text
DONE,<SEQ>
```

## 6. ESP8266 发给 51 的串口协议

ESP8266 -> 51：

```text
#STOP
#RUN,FWD,180
#RUN,BACK,180
#RUN,LEFT,160
#RUN,RIGHT,160
#RUN,SPINL,180
#RUN,SPINR,180
```

串口参数：

- 波特率：`9600`
- 数据位：`8`
- 停止位：`1`
- 校验位：`None`

## 7. 51 侧的保护逻辑

- 收到合法命令立即执行
- 收到非法命令直接忽略
- 超过 `1000 ms` 没有收到新命令时，自动停车
