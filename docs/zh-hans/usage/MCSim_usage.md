# MCSim使用说明

本文介绍MCSim的使用方法，包括安装、编译、运行和参数配置等。

本版本的 RTL 对齐目标为 **Baiyang-V1.0**。

## 安装和编译

需要 GNU Make 和支持 C++20 的 `g++`。以下命令获取 GitHub 的 v1.1 发布分支；已有源码可直接编译。

```shell
git clone --branch MCSim-v1.1 https://github.com/OpenXiangShan/MCSim
cd MCSim
make -j$(nproc)
```

编译结果为`./mcsim`。

其他构建目标如下，均在项目根目录执行：

| 用途 | 命令 | 产物 |
| --- | --- | --- |
| Verilator 库 | `make -j$(nproc) verilator-lib` | `build-verilator-lib/libmcsim.so` 和 `libmcsim.a` |
| C API 冒烟测试 | `make -j$(nproc) verilator-lib-smoke` | 构建并运行 `build-verilator-lib/mcsim_c_api_smoke` |
| gem5 库 | `make -j$(nproc) lib GEM5_DIR=/path/to/gem5` | `/path/to/gem5/ext/mcsim/libmcsim.so` |

gem5 目标使用 C++17 和 GCC 的 `-fconcepts`；将 `/path/to/gem5` 替换为实际目录，并先创建其 `ext/mcsim/` 子目录。

## 运行

有两种运行模式，由`-d`选项指定，`1`和`2`分别为trace驱动和随机驱动。

trace驱动模式，需要指定trace文件：

```shell
./mcsim -d 1 -t <trace路径>
```

当前默认 `NEW_TRACE = true`，每行格式为 `id dependency timestamp R/W addr`，用空白分隔。例如：

```text
0 0 0 R 0x80000000
1 0 4 W 0x80000040
```

依次为事务 ID、向前依赖距离（`0` 表示无依赖）、时间戳、读写类型和地址。前三项为十进制，地址为十六进制；时间戳乘以 `timestamp_x` 后对应 MC 周期。

旧格式为三列 `timestamp R/W addr`，使用时需设置 `include/config.h` 中的 `NEW_TRACE = false` 并重新编译。`USE_DEPENDENCY` 默认是 `false`，仅控制依赖约束，不改变输入列数。使用 `mc_trace/` 中的 trace 或脚本生成的 trace 时，请确认其格式与配置一致，工具说明见[脚本使用说明](scripts_usage.md)。

随机驱动模式，需要指定产生读的比例：

```shell
./mcsim -d 2 -r 0.9  # 90%读
```

随机模式可用 `-n <数量>` 设置请求数，默认值为 `1000000`；当前实现中 `-n` 不限制 trace 模式读取的条数。

## 参数配置

参数配置文件为`include/config.h`，目前所有模块共用一个`config.h`，因此修改配置后所有模块均需重新编译。

一些关键参数如下：

- **`freq`**：模拟MC时钟频率，单位MHz，当前默认是 `800`。
- **`TimingConfig`**：DFI时序参数。
- **`timestamp_x`**：用于调整时间戳间隔，当前默认是 `1`。
- **`NUM_CHANNELS`**：通道数，当前默认是 `2`。
- **`vc_num`**：AXI2UI中虚通道的个数。（单个虚通道内部保序返回axi id，不同虚通道之间乱序返回axi id）
- **`filter_mode`**：当前默认是 `ALL2SCHED`，请求直接进入 Scheduler。
- **`CACHE_EN`**：Cache开关，当前默认是 `false`。
- **`PrefetcherConfig.type`**：Cache子模块prefetcher的类型，为`NONE`时表示关闭prefetcher。
- **`DumpConfig`**：用于控制mcsim打印的信息。其中，**`DUMP_MODULEWISE_LOG`**开启时，会将各个模块间的关键信号传输事件以csv格式记录在指定目录下，类似于波形。

此外，还有一些其他细粒度的参数设置，详情可查看`include/config.h`。

## 导出模块粒度的日志

1. 修改`include/config.h`，开启**DUMP_MODULEWISE_LOG**，重新编译并运行；（由于要频繁地写日志文件，该选项打开时，会严重拖慢mcsim运行速度，因此一般情况下推荐关闭该选项）
2. 在项目根目录下，执行 **make latency**；
3. 查看生成的csv文件 `logs/latency.csv`。

运行时可通过 `-l run.logs/example` 指定日志目录；该选项本身不会启用模块日志。`make latency` 固定读取 `logs/`，自定义目录需直接调用脚本：

```shell
python3 scripts/dump_latency.py -in run.logs/example/in_cycle \
  -out run.logs/example/out_cycle -o run.logs/example/latency.csv
```

## 输出结果说明

1. `trans_count`：总访存次数；`Done xxx cycles`：执行全部trace所用的总周期数。
2. （AXI2UI）`xxx_full`：表示AXI2UI中，读通道/写通道/rob满的周期数。
3. （CACHE）总/读/写命中率；`block_xxx`：因为各种原因而阻塞的周期数；`prefetch_xxx`：预取总次数、预取准确率和覆盖率。
4. （SCHEDULER）`total conflict`：读写冲突的周期数；`rw_switch_cnt`：读写切换的次数。
5. （SCG）`idle_xxx`：因为各种原因阻塞的周期数。`ref_zq`：刷新和zq校准；`rs`：行切换；`r2w`：读到写时序；`w2r`：写到读时序；`empty`：没有命令。`act_cnt`：下发到dfi的act命令的个数。

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](../../../LICENSE.txt).
