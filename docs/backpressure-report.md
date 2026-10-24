# 背压阶段记录
日期：2026-10-24

## 第一阶段：缓冲上限与背压
状态：已完成，先编译再测试，7/7 通过（0.39 秒）。原始记录：backpressure-stage1.log。

实现：每连接 outputBuffer_ 的有效待发送数据硬上限为 1 MiB；降到 512 KiB 或以下恢复读取。recv 长度不超过剩余空间，到达上限关闭 EPOLLIN/EPOLLRDHUP；发送排空后重新注册读事件。单次读回调最多读取 64 KiB，让其他连接有机会运行。保留原有终端日志。

修改文件：include/Channel.h、include/TcpConnection.h、src/TcpConnection.cpp、CMakeLists.txt；新增 tests/backpressure_test.cpp。新增只读缓冲长度、暂停状态接口便于验证。

| 要求 | 验证方式 | 结果 |
|---|---|---|
| 正常收发 | 非阻塞连接发送 hello，经过真实 EventLoop 事件分发核对回显；原有 TCP 集成测试也通过 | PASS |
| 积压不超过硬上限 | 缩小 socket 发送缓冲，客户端不读取，持续写至暂停；继续 1000 次发送尝试/事件循环，检查长度与暂停状态 | PASS，峰值 1048576 字节 |
| 恢复后解除背压 | 客户端重新读取，核对已接受的全部数据；验证读关注位恢复，并再次收发 hello | PASS，无数据丢失 |
| 其他客户端正常通信 | 慢连接保持暂停，同一 EventLoop 中另一连接完成 hello 回显 | PASS |

四项专项测试使用 AF_UNIX 非阻塞 socketpair，直接驱动实际 TcpConnection 和 EventLoop，避免 TCP 内核缓冲差异导致无法稳定触发背压。原有 TCP 集成测试另行覆盖 8 MiB 数据、慢读、并发、重连及半关闭。

范围说明：硬上限指应用层待发送数据长度，不等同于 std::string capacity、内核 socket 缓冲或进程总内存上限。仍未限制总连接数。同步终端输出仍可能阻塞事件循环；写回调暂未设置单轮字节预算，最多处理当前有界缓冲。未实现优雅停机。

复现：
```bash
cd /home/lhjwsl/aster_server
cmake -S . -B build-review
cmake --build build-review -j2
ctest --test-dir build-review -V
```

## 第二阶段：ASan/UBSan 验证
第一阶段记录已完成，开始检测器构建与测试。结果见后续追加记录。

第二阶段结果：Debug 检测器构建成功；7/7 测试通过，0.67 秒。四项背压测试均通过，峰值仍为 1048576 字节，未报告 ASan/UBSan 错误。原始编译及测试日志：backpressure-stage2.log。

命令：
```bash
cmake -S . -B build-engineering-sanitize -DCMAKE_BUILD_TYPE=Debug -DASTER_SANITIZERS=ON
cmake --build build-engineering-sanitize -j2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-engineering-sanitize -V
```
专项测试正常退出，可以执行析构和退出泄漏检查；TCP 集成测试结束时终止服务进程，其退出泄漏覆盖仍有限。本轮没有继续调整其他功能，也未替换用户已有运行进程。
