# AsterServer

基于 `epoll` 的 C++17 多线程 TCP 服务器，采用 **Reactor 模式 + 线程池** 架构。核心目标是**能跑在生产环境里的正确性**：连接生命周期的悬垂指针防护、写缓冲背压、信号安全退出，每一项都有对应的自动化测试兜底。

> 命名由来：aster（紫苑）——花语是「不会忘记」。对应本项目做的一件事：**不忘记回收每一个资源、不忘记关闭每一个 fd。**

---

## 目录

- [特性](#特性)
- [架构](#架构)
- [目录结构](#目录结构)
- [快速开始](#快速开始)
- [使用方式](#使用方式)
- [测试](#测试)
- [设计要点](#设计要点)
- [已知限制](#已知限制)

---

## 特性

| 能力 | 说明 |
|---|---|
| **单线程 Reactor** | 一个 `epoll` 实例 + 一个事件循环，所有 fd 事件在 loop 线程串行处理，天然无锁 |
| **跨线程任务投递** | `EventLoop::queueInLoop()` 线程安全，配合 `eventfd` 唤醒机制避免 epoll 空转阻塞 |
| **线程池异步业务** | 业务处理下沉到 `ThreadPool`，计算不阻塞 IO 事件循环 |
| **RAII 资源管理** | `Channel` 持有 fd 的注册关系，析构自动 `epoll_ctl(DEL)` |
| **背压保护** | 单连接输出缓冲上限 1 MB，触顶后暂停读事件，防止慢客户端撑爆内存 |
| **异步日志** | 独立写日志线程 + 有界队列（4096 条 / 1 MB），过载时**丢弃并计数**而非阻塞业务 |
| **信号安全退出** | `signalfd` 把 `SIGINT`/`SIGTERM` 变成普通 fd 事件，信号处理函数里不跑任何 C++ 代码 |
| **生命周期防悬垂** | `weak_ptr` 令牌校验异步回调，连接在任务执行中途被关闭也不会 use-after-free |

---

## 架构

```
                    ┌──────────────────────────────────────────┐
   客户端连接 ──────►│              EventLoop（主线程）           │
                    │                                          │
                    │   epoll_wait ──► Channel::handleEvent()   │
                    │        ▲                  │              │
                    │        │                  ├─► handleAccept  → TcpServer 接受连接
                    │        │                  ├─► handleRead    → TcpConnection 读数据
                    │        │                  └─► handleWrite   → TcpConnection 写数据
                    │        │                                 │
                    │   eventfd 唤醒（queueInLoop 投递的任务）     │
                    └────────┬─────────────────────────────────┘
                             │ submitRequest
                             ▼
                    ┌──────────────────┐        ┌───────────────────┐
                    │   ThreadPool     │───────►│  RequestProcessor │  纯业务计算，
                    │  （N 个工作线程）  │◄───────│   无 socket 访问    │  不碰任何 IO 对象
                    └──────────────────┘        └───────────────────┘
                             │ 结果回到 loop 线程（中间态检查 weak_ptr）
                             ▼
                    TcpConnection::completeRequest()
```

**关键分层约束**：`RequestProcessor::Handler` 的签名是 `std::string(const std::string&)`——它拿不到 fd、Channel、EventLoop 中的任何东西。这个设计是从**类型系统层面**杜绝「工作线程误操作 IO 对象」这类最难查的并发 bug。

---

## 目录结构

```
aster_server/
├── CMakeLists.txt              # 构建 + 全部 CTest 用例注册
├── Makefile                    # 快速单文件编译（开发期用）
├── include/
│   ├── Epoll.h                 # epoll_ctl 的 RAII 封装
│   ├── Channel.h               # fd + 关注事件 + 回调 的绑定单元
│   ├── EventLoop.h             # 事件循环 / 任务投递 / 唤醒
│   ├── TcpServer.h             # 监听、accept、连接表管理、背压准入
│   ├── TcpConnection.h         # 单连接读写缓冲、状态机、请求分发
│   ├── ThreadPool.h            # 通用线程池，submit 返回 future
│   ├── Logger.h                # 异步日志，有界队列，丢弃计数
│   ├── RequestProcessor.h      # 纯业务接口（可注入自定义 Handler）
│   └── setNotBlocking.h        # fd 非阻塞工具
├── src/
│   ├── main.cpp                # 入口：端口解析、signalfd、启动
│   ├── Epoll.cpp / Channel.cpp / EventLoop.cpp
│   ├── TcpServer.cpp / TcpConnection.cpp
│   ├── ThreadPool.cpp / Logger.cpp / RequestProcessor.cpp
│   ├── setNotBlocking.cpp
│   └── demo/logger_test.cpp
└── tests/
    ├── event_loop_test.cpp         # 回调注册与分发
    ├── event_loop_wakeup_test.cpp  # 跨线程唤醒正确性
    ├── lifecycle_test.cpp          # 连接生命周期（含析构顺序）
    ├── backpressure_test.cpp       # 4 种背压场景
    ├── thread_pool_test.cpp        # 并发提交、结果、异常
    ├── business_processing_test.cpp# 7 种业务场景
    ├── integration.py              # 端到端集成测试
    ├── logger_shutdown.py          # 日志优雅关闭
    └── business_cli.py             # 命令行业务验证
```

---

## 快速开始

### 依赖

- Linux（依赖 `epoll`、`signalfd`、`eventfd`，**不支持 Windows/macOS**）
- CMake ≥ 3.16、GCC/Clang（C++17）
- Python 3（仅运行 Python 集成测试时需要）

### 编译

```bash
git clone https://github.com/luojie000/aster_server.git
cd aster_server
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

产物：`build/aster_server`

**开发期想更快**：`make` 直接生成单文件可执行程序。

### 开 sanitizer 跑一遍（强烈建议）

```bash
cmake -B build-asan -S . -DASTER_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j$(nproc)
cd build-asan && ctest --output-on-failure
```

`ASTER_SANITIZERS=ON` 会启用 **AddressSanitizer + UndefinedBehaviorSanitizer**。本项目大量涉及手写 fd 生命周期，ASan 是发现悬垂访问最有效的手段。

---

## 使用方式

### 启动

```bash
./build/aster_server          # 默认端口 8080，回显模式
./build/aster_server 9000     # 指定端口
./build/aster_server 9000 --calc   # 开启计算业务
```

启动后向 stderr 输出一行（同步、首行），便于脚本/测试解析端口：

```
LISTENING 8080
```

> 传端口 `0` 让内核分配随机端口时，请从这行输出读取实际端口。

### 测试连接

```bash
# 终端 1
./build/aster_server 8080 --calc

# 终端 2
nc 127.0.0.1 8080
> 12+30
42
```

### 退出

`Ctrl+C` 或 `kill <pid>` 均可。二者都走同一条路径：

```
signalfd 收到信号 → 记录日志 → loop.stop() → 连接析构 → ThreadPool 排空 → Logger 落盘退出
```

整个过程不会丢日志、不会触发 SIGSEGV。

### 自定义业务

不传 `--calc` 时是回显模式。想接入自己的业务，实现一个纯函数：

```cpp
#include "EventLoop.h"
#include "TcpServer.h"
#include "RequestProcessor.h"

int main() {
    EventLoop loop;
    TcpServer server(loop, 8080, [](const std::string& req) -> std::string {
        // 只有字符串进、字符串出；不能访问 socket / Channel / EventLoop
        return "echo: " + req;
    });
    server.start();
    loop.run();
}
```

**约束**：自定义 Handler 必须**有界**（有超时或规模上限）且**可并发调用**（多个工作线程同时执行）。

---

## 测试

项目共有 **26 个 CTest 用例**，覆盖从单元到端到端四层：

```bash
cd build
ctest --output-on-failure          # 全部
ctest -R backpressure -V           # 只跑背压
ctest -R business -V               # 只跑业务
```

| 测试组 | 用例数 | 覆盖内容 |
|---|---|---|
| `event_loop_*` | 2 | 回调分发、跨线程 `queueInLoop` 唤醒、`eventfd` 语义 |
| `connection_lifecycle` | 1 | 连接建立→读写→关闭全链路，析构顺序与 fd 泄漏 |
| `backpressure_1..4` | 4 | 输出缓冲触顶暂停读、恢复读、超限断连、并发多连接 |
| `thread_pool` | 1 | 并发提交、`future` 取值、任务异常传播 |
| `logger_*` | 2 | 队列溢出丢弃计数、关闭时日志不丢 |
| `business_*` | 8 | 处理器注入、成功路径、多客户端、断连、关闭、配额上限、错误处理 |
| `reactor_integration` | 1 | 真实 socket 端到端（Python 驱动） |

---

## 设计要点

### 1. 为什么用 `signalfd` 而不是 `signal()` handler

在信号处理函数里调用 `printf`、`mutex::lock`、`malloc` 都是**未定义行为**（异步信号不安全）。本项目在 `main()` 一开始就先 `pthread_sigmask` 屏蔽信号，再创建 `signalfd` 加入 epoll：

```cpp
// 必须在创建 Logger 之前屏蔽，让日志线程继承掩码
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGTERM);
pthread_sigmask(SIG_BLOCK, &mask, &previous_);
fd_ = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
```

这样信号就退化成了普通的可读事件，优雅退出逻辑跑在正常栈上，可以随便加锁、写日志。

### 2. 背压：宁可拒绝，也不 OOM

`TcpConnection::kOutputLimit = 1 MB`。当 `outputBuffer_` 超过阈值：

1. 暂停该连接的 `EPOLLIN`（`readPaused_ = true`）
2. 对端继续发送时，内核接收缓冲区填满，TCP 窗口收缩为 0，压力自然传导回发送方
3. 待 `handleWrite` 排空缓冲后恢复读事件

`TcpServer::kMaxPendingRequests = 64` 同理——这是**准入控制**，超出就直接拒绝，而不是无限制地把请求堆进线程池队列。

### 3. `weak_ptr` 令牌解决「回调执行到一半连接没了」

典型竞态：

```
loop 线程：收到请求 → 投递给线程池
loop 线程：该客户端断连，TcpConnection 析构
工作线程：任务执行完毕 → 试图 write 到已析构的连接  ❌ crash
```

解法：每个 `TcpConnection` 持有一个 `shared_ptr<int> lifetime_`。投递任务时传 `weak_ptr` 副本，工作线程返回后先 `lock()` 校验：

```cpp
void TcpServer::submitRequest(int fd, std::weak_ptr<int> life, std::string request) {
    workers_->submit([this, fd, life, req = std::move(request)]() {
        auto response = processor_(req);        // 纯计算，不碰 IO
        loop_.queueInLoop([this, fd, life, resp = std::move(response)]() {
            if (life.expired()) return;         // 连接已死，安静丢弃
            // ... 安全地写回
        });
    });
}
```

### 4. 日志过载策略：绝不阻塞业务线程

`Logger` 是有界队列（4096 条 / 1 MB）。队列满时**直接丢弃并累加 `droppedCount()`**，而不是让业务线程阻塞等待磁盘 IO。这条策略牺牲少量日志，换取「日志系统故障不会拖垮主服务」——对服务器来说这是正确的取舍。

### 5. 线程归属约定

代码里不是靠注释「提醒」大家守规矩，而是靠这几个事实约束：

| 对象 | 线程约束 |
|---|---|
| `EventLoop::run()` / `stop()` / Channel 增删 | **仅 loop 线程** |
| `EventLoop::queueInLoop()` | 任意线程（内部加锁 + eventfd 唤醒） |
| `TcpConnection` 全部方法 | **仅 loop 线程** |
| `RequestProcessor::Handler` | 线程池线程，可并发，禁止访问 IO 对象 |
| `Logger::log()` | 任意线程 |
| `Logger::stop()` | 需先停止所有生产者，串行调用 |

---

## 已知限制

- **仅 Linux**：`epoll` / `signalfd` / `eventfd` 都是 Linux 专有。移植到 macOS 需换 kqueue，Windows 需 IOCP。
- **单 Reactor**：所有事件处理都在一个线程串行。高并发下 loop 线程可能成为瓶颈，进一步优化方向是 **主从 Reactor**（multiple epoll + 连接按 fd 取模分配）。
- **无 TLS**：当前是明文 TCP。要上 HTTPS 需要在 `TcpConnection` 层接 OpenSSL。
- **协议极简**：按行分隔（`kLineLimit = 64`），不是 HTTP。作为教学/实验性的 reactor 骨架，而不是通用 Web 服务器。
- **连接表是 `unordered_map` 线性扩容**：连接数上万时建议换更合适的数据结构。
- **仓库内含构建目录**：根目录的 `build-hardening-baseline/`、`build-logger-integration/`、`build-business-sanitize/` 等是历史构建产物，`.gitignore` 已覆盖但未从索引移除，克隆时会多下载十余 MB。建议执行：

  ```bash
  git rm -r --cached build-hardening-baseline build-logger-integration
  git commit -m "chore: 移除误提交的构建产物"
  ```

---

## 相关文档

`docs/` 目录下有更深入的设计与测试记录，推荐在读完本 README 后按需查阅：

| 文档 | 内容 |
|---|---|
| [docs/backpressure-report.md](docs/backpressure-report.md) | 背压机制的设计与四组压测结果分析 |
| [docs/logger-integration.md](docs/logger-integration.md) | 异步日志接入事件循环的过程与取舍 |
| [docs/logger-test-report.md](docs/logger-test-report.md) | 日志系统测试报告（含过载丢弃验证） |
| [docs/thread_pool_integration.md](docs/thread_pool_integration.md) | 线程池与 reactor 的协作方式、任务生命周期管理 |

---

## License

未指定。如需开源授权请补充 LICENSE 文件。
