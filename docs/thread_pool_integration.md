# AsterServer：业务线程池的第一次安全接入

已在 WSL `/home/lhjwsl/aster_server` 检查实际源码、修改并重新验证（2026-11-01）。本轮开始时仓库已有未提交的线程池接入实现和测试，实际基线为 18/18。本轮保留这些工作，调整关闭监听的顺序，补充多客户端测试及停机监听关闭断言，并更新 CMake 与本文档。最终完整测试为 19/19，没有削弱现有测试，也未进行性能基准测试。

本轮直接修改四个文件：`src/TcpServer.cpp`、`tests/business_processing_test.cpp`、`CMakeLists.txt`、`docs/thread_pool_integration.md`。下方文件表列出的是整个接入方案涉及的文件，包含本轮开始前已经存在的改动；没有将它们冒充为本轮新写的代码。无需重构 Reactor 或连接所有权。

## 运行

在 WSL 的第一个终端运行：

```bash
cd /home/lhjwsl/aster_server
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
./build/aster_server 8080 --calc
```

第二个终端可用 Python 标准库发送请求：

```bash
python3 - <<'PY'
import socket
with socket.create_connection(('127.0.0.1', 8080), timeout=3) as client:
    client.sendall(b'CALC 10\nCALC 1000000\n')
    client.shutdown(socket.SHUT_WR)
    with client.makefile('r') as replies:
        print(replies.read(), end='')
PY
```

预期输出：

```text
RESULT 4
RESULT 78498
```

`CALC n` 表示统计 2 到 n 之间的素数数量。使用筛法，有实际 CPU 计算工作；n 限制在 0..1000000，计算所用容器为每个任务独有。不带 `--calc` 仍运行原有二进制回显服务。终端按 Ctrl+C 或发送 SIGTERM 可正常结束。

## 回调流与线程职责

```text
EventLoop 线程
  TcpConnection::handleRead() 接收字节
  dispatchRequest() 按换行符取出一条请求
  TcpServer::submitRequest() 向线程池提交请求副本
        ↓
ThreadPool 工作线程
  RequestProcessor::process() 解析命令、计算结果
  EventLoop::queueInLoop() 把结果回调放回事件循环
        ↓
EventLoop 线程
  检查服务器寿命标记
  检查连接寿命标记、connections_ 中的 fd 与标记身份
  TcpConnection::completeRequest() 缓存响应
  handleWrite() 发送数据、更新 Channel、提交下一条请求
```

初学者可以把 EventLoop 理解为负责收发的工作人员，把 ThreadPool 理解为负责计算的工作人员。计算人员拿到的是请求内容副本，算完后把结果交回；套接字读写、输出缓冲区和 Channel 始终由 EventLoop 线程管理。EventLoop 不调用 `future.get()` 等待计算结果，所以仍可接待其他连接。

默认两个业务工作线程。同一连接只允许一个任务处于处理中，后续请求暂存在有限输入缓冲区中，响应自然保持请求顺序。不同连接可同时计算。这是便于学习和验证的第一步，并未实现同一连接内的并行计算和结果重排。

## 为什么旧任务不会访问已销毁连接

- `TcpServer` 继续使用 `unordered_map<int, unique_ptr<TcpConnection>>`，没有改为共享拥有连接。
- 每个连接有独立寿命标记；关闭时立即失效，即使连接表稍后才删除对象，旧结果也不能再使用它。
- 工作任务和结果回调都不捕获 `TcpConnection*`。结果回调携带 fd 和弱标记，回到循环线程后才检查连接表。
- fd 可能被操作系统重新分配给另一个连接；只有 fd 相同还不够，标记身份也必须一致。
- 结果回调中的服务器地址只有在服务器弱寿命标记仍有效时才会访问。这个检查和服务器销毁都由同一循环线程完成，因此不会在“刚检查完”时被另一线程销毁。
- 工作线程使用的 `EventLoop*` 由明确销毁顺序保护：服务器先移除监听 Channel 并关闭监听 fd，再让寿命标记失效，随后调用线程池 `stop()` 排空并等待所有工作线程退出；之后才允许销毁 EventLoop。工作线程不会等待循环线程，所以此处等待不会形成互相等待。
- Logger 沿用既有关闭顺序：服务器与业务工作线程退出后才停止 Logger。

服务器及连接必须在循环线程中使用和销毁，或在循环已经停止时销毁；EventLoop 必须比服务器活得更久。此约定写入头文件。

## 协议与边界

| 项目 | 当前约定 |
|---|---|
| 请求 | `CALC <十进制无符号整数>\n`，严格使用 LF 换行 |
| 数值 | 0..1000000；不接受负数、额外参数或尾随空格 |
| 成功响应 | `RESULT <素数数量>\n` |
| 命令或数值错误 | `ERR command\n` 或 `ERR range 0..1000000\n` |
| 单行上限 | 不含换行符最多 64 字节；超过则关闭连接 |
| 每连接输入缓冲 | 最多 4096 字节；达到容量且未能消费时关闭连接 |
| 每连接进行中任务 | 最多 1 个，保证响应顺序 |
| 每服务器已提交、尚未回收的任务 | 最多 64 个，包含已经计算完但回调尚未执行的任务 |
| 任务达到上限 | 返回 `ERR busy\n`；客户端可稍后重试 |
| 单条响应 | 最多 128 字节，超长处理器结果改为错误响应 |
| 输出缓冲 | 保留原有 1 MiB 上限，提交下一请求前预留响应空间 |
| 客户端发送 FIN | 视为半关闭：完成完整请求并发送响应，再关闭 |
| FIN 前最后一个不完整行 | 丢弃没有换行符的尾部片段 |
| 客户端重置连接 | 删除连接，后台任务可以完成，但结果会丢弃 |
| 服务器关闭 | 完成已接收的计算并等待线程退出，不保证关闭中的响应全部发出 |

自定义处理函数必须有限时长、可并发调用，不能触碰网络对象或等待 EventLoop。当前内置计算满足这些条件。任务异常转换为 `ERR processing\n`，避免请求永久卡在“处理中”。这一步没有引入全局连接数量限制或任务取消机制。

## 本次文件变更

以下路径均以 `/home/lhjwsl/aster_server` 为根目录：

| 文件 | 作用 |
|---|---|
| `include/RequestProcessor.h`、`src/RequestProcessor.cpp`（新增） | 独立业务处理接口与有界素数计算 |
| `include/TcpConnection.h`、`src/TcpConnection.cpp` | 按行组包、请求回调、寿命标记、顺序响应和输入上限 |
| `include/TcpServer.h`、`src/TcpServer.cpp` | 拥有线程池、提交请求、验证结果目标、限制任务数、等待工作线程退出 |
| `src/main.cpp` | 显式 `--calc` 模式 |
| `CMakeLists.txt` | 链接新增实现并注册测试，保留已有 ThreadPool 测试项 |
| `.gitignore` | 忽略本次新增的内存检查构建目录 |
| `tests/business_processing_test.cpp`（新增） | 七个带超时的业务与生命周期测试；仅在循环线程读取内部状态以精确同步 |
| `tests/business_cli.py`（新增） | 实际可执行文件的协议、跨线程唤醒与 SIGTERM 正常退出测试 |
| `docs/thread_pool_integration.md`（新增） | 本说明的仓库副本 |

`ThreadPool`、`EventLoop`、`Logger` 的实现未改动。既有测试文件未改动。

## 实际验证结果

| 验证 | 结果 | CTest 总耗时 |
|---|---|---:|
| 本轮修改前实际基线 | 18/18 通过 | 0.89 秒 |
| 业务定向测试 | 8/8 通过 | 0.51 秒 |
| 修改后完整测试 | 19/19 通过 | 0.88 秒 |
| AddressSanitizer + UndefinedBehaviorSanitizer 完整测试 | 19/19 通过，未报告错误 | 2.11 秒 |

当前业务测试覆盖（包括已有测试与本轮补充）：

1. `business_processor`：已知素数数量、上下界和非法输入。
2. `business_success`：拆包、同连接多个请求顺序、半关闭、计算确实运行在工作线程。
3. `business_disconnect`：用同步关卡阻塞旧任务，等旧连接从连接表删除，再确认新连接复用同一 fd；另一工作线程可以处理新请求，释放旧任务后旧结果不会发往新连接；另覆盖任务阻塞时输入缓冲容量和单连接任务数量限制。
4. `business_shutdown`：两个任务正在执行、一个任务排队时销毁服务器；确认等待任务时监听端口已经拒绝新连接，全部工作完成并退出，随后执行服务器销毁后遗留的回调仍安全。
5. `business_limits`：64 个任务占满时，第 65 个收到 busy，任务完成后可继续处理；过长请求行关闭连接。
6. `business_errors`：业务异常、过长响应、错误命令、非法数值、最后的不完整行，以及后续有效请求恢复。
7. `business_cli`：真实 CLI 启动，正常阻塞的 EventLoop 被工作线程结果唤醒，按序接收响应并半关闭，SIGTERM 退出码为 0。

8. `business_multiclient`（本轮新增）：两个客户端各自拆分请求，同时占用两个工作线程；多条连续请求按连接保持顺序，不串线、不重复、不丢失，半关闭后读到完整响应和 EOF。

C++ 测试每阶段等待最多 3 秒、进程硬超时 12 秒、CTest 超时 15 秒。CLI 测试有独立启动、套接字和进程退出超时，CTest 上限 20 秒。关键断线和关闭场景由同步关卡控制，不依赖计算恰好够慢。

复现命令：

```bash
cd /home/lhjwsl/aster_server
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build -R '^business_' --output-on-failure
ctest --test-dir build --output-on-failure

cmake -S . -B build-business-sanitize -DBUILD_TESTING=ON \
    -DASTER_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-business-sanitize -j2
ctest --test-dir build-business-sanitize --output-on-failure
```

测试结果验证了上述场景；ASan/UBSan 不等同于线程竞争检测。这次的线程安全主要依靠明确的单线程归属、寿命检查与退出顺序。
