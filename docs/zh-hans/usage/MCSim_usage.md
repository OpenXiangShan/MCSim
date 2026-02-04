# MCSim使用说明

本文介绍MCSim的使用方法，包括安装、编译、运行和参数配置等。

## 安装和编译

```shell
git clone https://github.com/OpenXiangShan/MCSim
cd MCSim
make -j$(nproc)
```

编译结果为`./mcsim`。

## 运行

有两种运行模式，由`-d`选项指定，`1`和`2`分别为trace驱动和随机驱动。

trace驱动模式，需要指定trace文件：

```shell
./mcsim -d 1 -t <trace路径>
```

`mc_trace`目录下已有trace示例，格式：`tm, r/w, addr`，其中tm代表时间戳，r/w代表读写，addr代表访存地址。此外，也可以用scripts下的脚本生成随机/顺序trace。


随机驱动模式，需要指定产生读的比例：

```shell
./mcsim -d 2 -r 0.9  # 90%读
```

## 参数配置

参数配置文件为`include/config.h`，目前所有模块共用一个`config.h`，因此修改配置后所有模块均需重新编译。

一些关键参数如下：

- **`freq`**：模拟MC时钟频率，单位MHz。
- **`TimingConfig`**：DFI时序参数。
- **`timestamp_x`**：用于调整时间戳间隔。
- **`vc_num`**：AXI2UI中虚通道的个数。（单个虚通道内部保序返回axi id，不同虚通道之间乱序返回axi id）
- **`filter_mode`**：当前默认是`ALL2CACHE`，表示**当`CACHE_EN`开启时，命令全进Cache**（但如果`CACHE_EN`没有开启，命令依旧不进Cache）。
- **`CACHE_EN`**：Cache开关。
- **`PrefetcherConfig.type`**：Cache子模块prefetcher的类型，为`NONE`时表示关闭prefetcher。
- **`DumpConfig`**：用于控制mcsim打印的信息。其中，**`DUMP_MODULEWISE_LOG`**开启时，会将各个模块间的关键信号传输事件以csv格式记录在指定目录下，类似于波形。

此外，还有一些其他细粒度的参数设置，详情可查看`include/config.h`。

## 导出模块粒度的日志

1. 修改`include/config.h`，开启**DUMP_MODULEWISE_LOG**，重新编译并运行；（由于要频繁地写日志文件，该选项打开时，会严重拖慢mcsim运行速度，因此一般情况下推荐关闭该选项）
2. 在项目根目录下，执行 **make latency**；
3. 查看生成的csv文件 `logs/latency.csv`。

## 输出结果说明

1. `trans_count`：总访存次数；`Done xxx cycles`：执行全部trace所用的总周期数。
2. （AXI2UI）`xxx_full`：表示AXI2UI中，读通道/写通道/rob满的周期数。
3. （CACHE）总/读/写命中率；`block_xxx`：因为各种原因而阻塞的周期数；`prefetch_xxx`：预取总次数、预取准确率和覆盖率。
4. （SCHEDULER）`total conflict`：读写冲突的周期数；`rw_switch_cnt`：读写切换的次数。
5. （SCG）`idle_xxx`：因为各种原因阻塞的周期数。`ref_zq`：刷新和zq校准；`rs`：行切换；`r2w`：读到写时序；`w2r`：写到读时序；`empty`：没有命令。`act_cnt`：下发到dfi的act命令的个数。

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](LICENSE).