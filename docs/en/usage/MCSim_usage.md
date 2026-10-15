# MCSim Usage Guide

This document describes how to use MCSim, including installation, compilation, execution, and parameter configuration.

This version targets the **Baiyang-V1.0** RTL.

## Installation and Compilation

GNU Make and a C++20-capable `g++` are required. The commands below select the GitHub v1.1 release branch; an existing checkout can be built directly.

```shell
git clone --branch MCSim-v1.1 https://github.com/OpenXiangShan/MCSim
cd MCSim
make -j$(nproc)
```

The compiled binary is `./mcsim`.

Other build targets, run from the project root:

| Purpose | Command | Output |
| --- | --- | --- |
| Verilator libraries | `make -j$(nproc) verilator-lib` | `build-verilator-lib/libmcsim.so` and `libmcsim.a` |
| C API smoke test | `make -j$(nproc) verilator-lib-smoke` | Builds and runs `build-verilator-lib/mcsim_c_api_smoke` |
| gem5 library | `make -j$(nproc) lib GEM5_DIR=/path/to/gem5` | `/path/to/gem5/ext/mcsim/libmcsim.so` |

The gem5 target uses C++17 and GCC's `-fconcepts`. Replace `/path/to/gem5` with the actual directory and create its `ext/mcsim/` subdirectory first.

## Execution

There are two execution modes, specified by the `-d` option: `1` for trace-driven mode and `2` for random-driven mode.

Trace-driven mode requires specifying a trace file:

```shell
./mcsim -d 1 -t <trace_file_path>
```

The default is `NEW_TRACE = true`. Each line has five whitespace-separated fields: `id dependency timestamp R/W addr`. For example:

```text
0 0 0 R 0x80000000
1 0 4 W 0x80000040
```

The fields are the transaction ID, backward dependency distance (`0` means no dependency), timestamp, read/write operation, and address. The first three fields are decimal; the address is hexadecimal. The timestamp multiplied by `timestamp_x` gives the MC cycle.

Legacy traces have three fields: `timestamp R/W addr`. Set `NEW_TRACE = false` in `include/config.h` and rebuild to use them. `USE_DEPENDENCY` defaults to `false` and controls dependency constraints without changing the number of input fields. Match the configuration to the format of traces from `mc_trace/` or the scripts; see the [Scripts Usage Guide](scripts_usage.md).

Random-driven mode requires specifying the proportion of read operations:

```shell
./mcsim -d 2 -r 0.9  # 90% reads
```

In random mode, `-n <count>` sets the request count, which defaults to `1000000`. The current implementation does not use `-n` to limit the number of trace records read.

## Parameter Configuration

The parameter configuration file is `include/config.h`. All modules currently share a single `config.h` file, so any configuration changes require recompiling all modules.

Some key parameters are as follows:

- **`freq`**: Simulated memory controller clock frequency, in MHz; currently defaults to `800`.
- **`TimingConfig`**: DFI timing parameters.
- **`timestamp_x`**: Used to adjust the timestamp interval; currently defaults to `1`.
- **`NUM_CHANNELS`**: Number of memory channels; currently defaults to `2`.
- **`vc_num`**: Number of virtual channels in AXI2UI. (AXI IDs are returned in order within a single virtual channel but out-of-order across different virtual channels).
- **`filter_mode`**: Currently defaults to `ALL2SCHED`, which routes requests directly to the Scheduler.
- **`CACHE_EN`**: Cache enable/disable switch; currently defaults to `false`.
- **`PrefetcherConfig.type`**: Type of the prefetcher in the Cache submodule. Set to `NONE` to disable the prefetcher.
- **`DumpConfig`**: Controls the information printed by mcsim. When **`DUMP_MODULEWISE_LOG`** is enabled, key inter-module signal transmission events are recorded in CSV format in the specified directory, similar to a waveform.

Additionally, there are other fine-grained parameter settings. For details, please refer to `include/config.h`.

## Exporting Module-Level Logs

1. Modify `include/config.h` to enable **DUMP_MODULEWISE_LOG**, then recompile and run. (Enabling this option will significantly slow down mcsim execution due to frequent log file writes. Therefore, it is recommended to keep this option disabled under normal circumstances.)
2. Execute **make latency** in the project root directory.
3. Check the generated CSV file `logs/latency.csv`.

Use `-l run.logs/example` at runtime to select a log directory; this option does not enable module-level logging by itself. `make latency` always reads `logs/`. For a custom directory, invoke the script directly:

```shell
python3 scripts/dump_latency.py -in run.logs/example/in_cycle \
  -out run.logs/example/out_cycle -o run.logs/example/latency.csv
```

## Output Results Explanation

1. `trans_count`: Total number of memory accesses; `Done xxx cycles`: Total cycles used to execute all traces.
2. (AXI2UI) `xxx_full`: Number of cycles where the AXI2UI read channel/write channel/ROB was full.
3. (CACHE) Overall/read/write hit rates; `block_xxx`: Number of cycles blocked due to various reasons; `prefetch_xxx`: Total prefetch count, prefetch accuracy, and coverage.
4. (SCHEDULER) `total conflict`: Number of cycles with read-write conflicts; `rw_switch_cnt`: Number of read-write switches.
5.  (SCG) `idle_xxx`: Number of cycles blocked for various reasons. `ref_zq`: Refresh and ZQ calibration; `rs`: Row switching; `r2w`: Read-to-write timing; `w2r`: Write-to-read timing; `empty`: No command. `act_cnt`: Number of ACT commands issued to the DFI interface.

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](../../../LICENSE.txt).
