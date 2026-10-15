# scripts使用说明

本文介绍`scripts/`目录下一些脚本的功能。

## **dump_latency.py**

和mcsim的日志模块配合使用，将 记录的中间日志 转化为 **每条trace在每个模块的延迟**，以csv格式导出到指定文件。当开启mcsim的 **`DUMP_MODULEWISE_LOG`** 选项生成日志后，在项目根目录下，执行`make latency`，即可调用该程序生成csv文件。

## **plot_latency.py**

将 `make latency` 生成的csv文件输入到 `plot_latency.py` 中，可以得到**访存的延迟分布图**。

## **plot_trace.py**

生成旧格式trace的**访存地址图**。

## **trace处理相关的脚本**

- **gen_random.py**：生成旧三列格式的随机trace。
- **gen_sequential.py**：生成旧三列格式的顺序trace。
- **cook_bin_trace.py**：执行 `python3 scripts/cook_bin_trace.py trace.bin`，将二进制 trace 转为新五列格式，输出 `trace.simple.new`。
- **gen_new_trace.py**：执行 `python3 scripts/gen_new_trace.py trace.simple`，为旧三列 trace 添加随机 ID 和依赖字段，输出 `trace.simple.new`；这些随机字段不能还原真实采集的 ID 和依赖。

新旧格式的配置方式见 [MCSim使用说明](MCSim_usage.md)。旧的预处理脚本 `cook_trace.py`、`cook_trace.sh` 保存在 `scripts/storage/`。

## **批量执行相关的脚本**

- **batch_run_trace.py**：在多台服务器上分布式编译并运行 trace，按 `PARAM_RANGES` 遍历参数组合，保存每条 trace 的运行结果，并生成 `best_configs.log` 和 `test_results_<trace>.xlsx`。
  - 使用前需要按环境修改脚本顶部的 `SSH_USER`、`SERVERS_IP`、`TRACE_NAMES`、`ROOT_DIR` 等配置，并保证各服务器可以SSH免密登录。
  - 普通模式需要先将脚本内的 `SPEC_SCORE_MODE` 设置为 `False`，再执行 `python3 scripts/batch_run_trace.py`。
  - 当前脚本中的 `SPEC_SCORE_MODE` 默认为 `True`，因此直接执行脚本会进入 SPEC 跑分模式。也可以执行 `python3 scripts/batch_run_trace.py --spec-score` 开启该模式；该模式会自动扫描 `SPEC_TRACE_BASE_DIR` 覆盖 `TRACE_NAMES`，并在全部 trace 完成后额外生成 `spec_score_report.csv`。如果 `PARAM_RANGES` 产生多个参数组合，报告会按 `Config` 和 `Param_*` 列分别输出每个组合的 `Score`、`Score_Per_GHz` 和 overall geomean。
- **scripts/storage/batch_run_trace.sh**：指定trace目录，对一种参数配置的mcsim可执行文件，运行目录下的所有trace。
- **scripts/storage/batch_run_compare.sh**：指定mcsim可执行文件目录和trace目录，对不同参数配置的mcsim可执行文件，运行目录下的所有trace。

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](../../../LICENSE.txt).
