# MCSim v1.1 更新说明

本文介绍 GitHub 发布版本从 **MCSim v1.0 到 v1.1** 的主要变化。

本版本的 RTL 对齐目标为 **Baiyang-V1.0**。

<!-- ## 版本编号与来源

本说明使用 GitHub 的对外发布版本号。GitLab 早期的 `v0.x` 标签属于此前的开发版本编号，继续保留用于追溯历史。

| 对外发布版本 | GitHub 分支 | GitLab 源码对应关系 |
| --- | --- | --- |
| v1.0 | `MCSim-v1.0` | 源码基线为 `facc551`，GitHub 在此基础上维护了文档等调整 |
| v1.1 | `MCSim-v1.1` | 来自 `open/v0.3-align-baiyang-v0.94` 开发线；计划在最终发布提交上新增注释标签 `v1.1` |

旧标签 `v0.3-align-baiyang-v0.94` 标记的是此前的开发里程碑。本次发布还包含该标签之后的 gem5、Verilator 适配及其他修复，因此它与新标签 `v1.1` 对应不同的代码快照。 -->

## 主要更新

### 多通道与 trace 驱动

- 增加双通道控制器建模，各通道分别维护 AXI2UI、Filter、AddrMap、Cache、Scheduler 和 SCG 组件。
- 增加按地址位选择通道及 XOR 通道映射配置。
- 增加包含事务 ID 和依赖信息的 trace 格式：`id dependency timestamp R/W addr`，并提供依赖跟踪机制。
- 调整多通道事务下发与依赖处理，修复跨通道依赖、时间戳检查及预期返回周期溢出等问题。

### Cache 与预取器

- 重构 Cache 流水线，引入 `MainPipe`、`UnreadyQueue` 等组件，完善未完成请求和回填处理。
- 增加 LRU 替换配置，修复不同 Cache 容量下索引和 tag 位宽计算、队列命中及统计问题。
- 扩展预取器实现，增加 Spatial、Best Offset、Next N Line、Signature Path 和 SmartScan，并调整 Stream 预取器。
- 调整预取请求的触发和过滤，减少对当前请求及已在途 cacheline 的重复预取。

### 调度、时序与返回路径

- 扩展行管理策略，增加动态行策略和推迟刷新策略，修复关闭行状态转换、刷新计时初始化及 `tRP` 检查等问题。
- 调整 Scheduler 的读写切换、水位线及命令站组织，支持按 rank 和 bank group 管理，并仲裁 Filter/Cache 的读写输入。
- 调整 AXI 突发拆分、token 分配、写响应和读返回时序等行为，以及与 Baiyang-V1.0 对应的配置参数。
- 重构 Cache/SCG 读数据返回路径，增加返回仲裁，并细化返回延迟建模。
- 修复仿真结束条件，使其等待各模块处理完在途请求。

### gem5 与 Verilator 集成接口

- 增加供 gem5 集成的共享库构建、请求接收能力查询、读写完成回调和统计重置接口。
- 增加 Verilator C API，提供初始化、时钟推进、请求提交、响应获取、统计输出和结束接口。
- 增加 Verilator 共享库、静态库构建目标，以及 C API 冒烟测试入口。

### 日志与实验工具

- 增加延迟分布统计，调整模块级日志事件和延迟分析脚本，并增加 `-l` 日志目录选项。
- 增加二进制 trace 转换、分段处理、时间戳提取和 SimPoint 权重处理等工具。
- 增加分布式批量运行与参数扫描脚本，支持 Excel 结果汇总、SPEC 评分及按参数组合生成评分报告。
- 补充开源版权头，整理中英文文档入口。

## 升级注意事项

### Trace 格式

当前配置默认 `NEW_TRACE = true`，读取五列格式：

```text
id dependency timestamp R/W addr
```

使用 v1.0 的三列 trace（`timestamp R/W addr`）时，需要将 `include/config.h` 中的 `NEW_TRACE` 改为 `false`，然后重新编译。`USE_DEPENDENCY` 独立控制是否启用依赖约束；关闭它不会改变 `NEW_TRACE` 所选择的输入格式。

### 参数配置

当前源码默认使用双通道、`freq = 800` MHz、`timestamp_x = 1`、`filter_mode = ALL2SCHED`，且 `CACHE_EN = false`。切换版本进行实验时，应核对 `include/config.h` 中的通道映射、时序、Cache 和预取器参数，保持实验条件可比。

面向 Baiyang-V1.0 的建模和参数调整仍有适用范围，具体说明见 `include/config.h`；这些调整不代表与所有 Baiyang 配置逐周期一致。

### 工具路径

旧版 `batch_run_trace.sh`、`batch_run_compare.sh`、`cook_trace.py` 和 `cook_trace.sh` 位于 `scripts/storage/`。继续使用这些工具时，需要更新本地调用路径。

分布式运行脚本 `scripts/batch_run_trace.py` 需要按使用环境设置服务器、trace 和输出目录；详细说明见[脚本使用说明](docs/zh-hans/usage/scripts_usage.md)。

## 构建入口

```bash
# 独立运行程序
make -j

# Verilator 共享库与静态库
make -j verilator-lib

# 构建并运行 C API 冒烟测试
make -j verilator-lib-smoke

# gem5 共享库（替换为实际 gem5 目录）
make -j lib GEM5_DIR=/path/to/gem5
```

更多运行与参数说明见 [MCSim 使用说明](docs/zh-hans/usage/MCSim_usage.md)。
