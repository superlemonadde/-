# 计算机网络实验 — UDP 可靠文件传输协议

基于 UDP socket 实现的可靠文件传输系统，支持两种传输模式。

## 项目结构

```
.
├── Makefile              # 编译脚本
├── client/
│   ├── client.c          # 客户端入口（解析参数、创建 socket）
│   └── sender.c          # 文件发送逻辑（分包、重传、校验）
├── server/
│   ├── server.c          # 服务端入口（监听端口、绑定地址）
│   └── receiver.c        # 文件接收逻辑（收包、ACK、写文件）
└── common/
    ├── protocol.h        # 协议定义（包结构、端口、模式等）
    ├── utils.h           # 工具函数声明
    └── utils.c           # 工具函数实现（校验和、计时、文件大小）
```

## 协议设计

- **传输层**：UDP（`SOCK_DGRAM`）
- **数据包大小**：`DU_SIZE` = 1024 字节（可在 `protocol.h` 中调整）
- **校验机制**：每个包的字节流累加校验和
- **确认机制**：ACK 包确认，序号从 0 开始递增

### 两种传输模式

| 模式 | 说明 |
|------|------|
| `variable` | 可变批大小，按 1 → 2 → 3 → 1 → … 循环 |
| `fixed` | 固定批大小，每批发送 2 个数据单元（DU） |

## 快速开始

### 编译

```bash
make
```

生成两个可执行文件：`server_app` 和 `client_app`

### 运行

```bash
# 1. 先启动服务端
./server_app

# 2. 再启动客户端（另一个终端）
./client_app <文件路径> variable   # 可变批大小模式
./client_app <文件路径> fixed      # 固定批大小模式
```

### 清理

```bash
make clean    # 删除可执行文件和 received.bin
```

## 主要配置项

所有可调参数在 `common/protocol.h` 中：

```c
#define DU_SIZE     1024         // 单个数据单元大小（字节）
#define SERVER_PORT 8888         // 服务器监听端口
#define SERVER_IP   "127.0.0.1"  // 服务器地址（本地测试用回环地址）
#define MODE_VARIABLE 1          // 可变批大小模式
#define MODE_FIXED    2          // 固定批大小模式
```

## 环境要求

- GCC 编译器
- Linux / WSL / macOS（需支持 `<arpa/inet.h>` 和 `<unistd.h>`）
- Windows 原生编译需使用 MinGW 或 WSL
