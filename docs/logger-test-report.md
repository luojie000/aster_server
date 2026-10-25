# Logger 独立测试学习报告

日期：2026-10-25

仓库：`/home/lhjwsl/aster_server`

测试文件：`src/demo/logger_test.cpp`

## 本步目标与结论

通过一个独立的 main 函数测试 Logger，不需要启动网络服务器。测试已从 tests 移入现有的 src/demo 目录，并使用新路径重新编译运行。7 项检查全部通过，退出码为 0；编译器未输出警告。

## 声明与实现核对

已检查 include/Logger.h、src/Logger.cpp 和 CMakeLists.txt。最初发现并修正了以下编译问题：

- workerLoop 的定义缺少 void，补齐后与头文件声明一致。
- cv.wait 改为 cv_.wait，与成员变量名称一致。
- mutex_ 声明为 mutable，使 droppedCount() const 能加锁读取计数。加锁会改变锁的内部状态，但不会改变 Logger 对外的日志数据。
- 显式包含 <chrono>，用于 system_clock。

保留原有 Logger 实现，只做上述必要修正。修改前的源码备份保存在本次工作区 work/before 下。CMakeLists.txt 未改动；当前 CMake 目标尚未包含 Logger，本步直接使用 g++ 编译独立示例。

## 如何重新运行

在 WSL Ubuntu 终端中执行：

```bash
cd /home/lhjwsl/aster_server
g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -Iinclude src/demo/logger_test.cpp src/Logger.cpp \
    -o /tmp/aster_logger_test
timeout 10s /tmp/aster_logger_test
echo $?
```

- -std=c++17：使用项目要求的 C++17 标准。
- -Wall -Wextra -Wpedantic：开启常用编译警告。
- -pthread：启用线程支持。
- -Iinclude：让编译器能找到 Logger.h。
- 两个 .cpp 一起编译：一个提供测试入口 main，另一个提供 Logger 的实现。
- -o：指定生成的可执行文件位置。
- timeout 10s：若程序卡住，最多等待 10 秒。
- echo $?：查看上一条命令的退出码，0 表示成功。

实际复验使用工作区下的临时可执行文件，并设置同样的 10 秒运行超时；上面的命令方便手动复现。

## 测试过程与结果含义

1. 刚启动时 droppedCount() 为 0。
2. 写入四种级别的普通日志和一条超长日志，均被接受，丢弃计数仍为 0。
3. 立即调用 stop()，返回后能读到全部 5 条日志，验证后台处理与停止时排空日志的配合。
4. 四种级别、正文和输出顺序正确。
5. 9000 字节的正文被截断到 8192 字节，并带有 ...[truncated] 后缀；截断不等于丢弃。
6. 顺序再次调用 stop()，正常返回，输出和计数不变。
7. 停止后再写入日志，不产生新输出，丢弃计数变为 1。

关键流程：log() 将日志入队并通知后台线程；workerLoop() 取出日志并输出；stop() 设置停止标记并通过 join() 等待线程结束。

workerLoop() 是私有函数，因此测试通过输出结果间接验证它。测试不使用 sleep 猜测等待时间，而是在 stop() 返回后检查结果。

## 测试中的三个流

- Logger 内的 ostringstream line：把时间、级别和正文拼成一行字符串。
- Logger 内的 clog：接收这行日志，正常情况下写向标准错误流。
- 测试内的 ostringstream captured：临时接收 clog 的内容，方便逐条核对；后台线程退出后才读取它，并恢复 clog 原来的输出位置。

测试会把四条普通日志打印出来方便观察，不会把整条超长日志刷到终端。PASS 信息通过 cout 显示。

## 验证边界

本步覆盖普通入队、后台输出、超长消息截断、停止时排空、顺序重复停止，以及停止后丢弃计数。未覆盖队列满时的丢弃、多生产者压力测试或多个线程同时调用 stop()，不能据此认为这些场景已验证。

## 新路径下的实际运行输出

```text
[PASS] initial droppedCount = 0
[PASS] accepted messages are not dropped
[PASS] workerLoop outputs all 5 messages before stop returns
2026-10-25 16:07:59 [DEBUG] debug message
2026-10-25 16:07:59 [INFO] hello logger
2026-10-25 16:07:59 [WARN] warning message
2026-10-25 16:07:59 [ERROR] error message
[PASS] log levels, message contents and FIFO order
[PASS] oversized message is truncated to 8192 bytes
[PASS] sequential stop calls are safe
[PASS] log after stop is rejected and droppedCount becomes 1
ALL TESTS PASSED
```
