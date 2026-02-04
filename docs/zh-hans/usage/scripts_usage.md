# scripts使用说明

本文介绍`scripts/`目录下一些脚本的功能。

## **dump_latency.py**

和mcsim的日志模块配合使用，将 记录的中间日志 转化为 **每条trace在每个模块的延迟**，以csv格式导出到指定文件。当开启mcsim的 **`DUMP_MODULEWISE_LOG`** 选项生成日志后，在项目根目录下，执行`make latency`，即可调用该程序生成csv文件。

## **plot_latency.py**

将 `make latency` 生成的csv文件输入到 `plot_latency.py` 中，可以得到**访存的延迟分布图**。

## **plot_trace.py**

生成旧格式trace的**访存地址图**。

## **trace处理相关的脚本**

- **gen_random.py**：生成随机trace。
- **gen_sequential.py**：生成顺序trace。

## **批量执行相关的脚本**

- **batch_run_trace.sh**：指定trace目录，对一种参数配置的mcsim可执行文件，运行目录下的所有trace。
- **batch_run_compare.sh**：指定mcsim可执行文件目录和trace目录，对不同参数配置的mcsim可执行文件，运行目录下的所有trace。

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](LICENSE).