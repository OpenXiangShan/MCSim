# MCSim Usage Guide

This document describes how to use MCSim, including installation, compilation, execution, and parameter configuration.

## Installation and Compilation

```shell
git clone https://github.com/OpenXiangShan/MCSim
cd MCSim
make -j$(nproc)
```

The compiled binary is `./mcsim`.

## Execution

There are two execution modes, specified by the `-d` option: `1` for trace-driven mode and `2` for random-driven mode.

Trace-driven mode requires specifying a trace file:

```shell
./mcsim -d 1 -t <trace_file_path>
```

Sample traces are provided in the `mc_trace` directory. The format is: `tm, r/w, addr`, where tm is the timestamp, r/w indicates read or write, and addr is the memory address. Alternatively, you can use the scripts under the scripts directory to generate random or sequential traces.


Random-driven mode requires specifying the proportion of read operations:

```shell
./mcsim -d 2 -r 0.9  # 90% reads
```

## Parameter Configuration

The parameter configuration file is `include/config.h`. All modules currently share a single `config.h` file, so any configuration changes require recompiling all modules.

Some key parameters are as follows:

- **`freq`**: Simulated memory controller clock frequency, in MHz.
- **`TimingConfig`**: DFI timing parameters.
- **`timestamp_x`**: Used to adjust the timestamp interval.
- **`vc_num`**: Number of virtual channels in AXI2UI. (AXI IDs are returned in order within a single virtual channel but out-of-order across different virtual channels).
- **`filter_mode`**: The current default is `ALL2CACHE`, meaning **when `CACHE_EN` is enabled, all commands go to the Cache** (but if `CACHE_EN` is not enabled, commands still do not enter the Cache).
- **`CACHE_EN`**: Cache enable/disable switch.
- **`PrefetcherConfig.type`**: Type of the prefetcher in the Cache submodule. Set to `NONE` to disable the prefetcher.
- **`DumpConfig`**: Controls the information printed by mcsim. When **`DUMP_MODULEWISE_LOG`** is enabled, key inter-module signal transmission events are recorded in CSV format in the specified directory, similar to a waveform.

Additionally, there are other fine-grained parameter settings. For details, please refer to `include/config.h`.

## Exporting Module-Level Logs

1. Modify `include/config.h` to enable **DUMP_MODULEWISE_LOG**, then recompile and run. (Enabling this option will significantly slow down mcsim execution due to frequent log file writes. Therefore, it is recommended to keep this option disabled under normal circumstances.)
2. Execute **make latency** in the project root directory.
3. Check the generated CSV file `logs/latency.csv`.


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