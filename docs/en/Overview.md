# Overview

MCsim models a memory controller. Its overall microarchitecture aligns with that of the YuQuan IP, but it simulates performance only, without processing actual data. Its primary purpose is to explore the performance impact of certain microarchitectural designs on the memory controller.

## Directory Structure

```
.
├── docs/           # Documentation
├── include/        # Header files
│   ├── config.h    # Configuration file (shared by all modules)
│   └── ...
├── Makefile        # Makefile
├── mc_trace/       # Traces
├── scripts/        # Utility scripts
└── src/            # Source code for modules
    ├── main.cc     # Program entry point
    └── ...
```

## Data Flow

![](../images/dataflow.png)

Unlike the YuQuan IP, MCsim simplifies the data return path during simulation. Data is returned directly from various modules to the AXI2UI module after a fixed, configurable delay.

## Brief Description of Module Functions

1. **`top`**：Processes traces and issues AXI commands to AXI2UI.
2. **`AXI2UI`**：Splits and combines AXI commands into UI (User Interface) commands, tags them with tokens (for ROB reordering), and dispatches them to the Filter. Currently, it only supports AXI requests with a burst length of 2.
3. **`Filter`**：Based on the Filter policy, sets flag bits for commands to determine which ones should be sent to the Cache and which should be sent directly to the Scheduler.
4. **`AddrMap`**：ranslates AXI addresses into SDRAM addresses according to different bg/bank/row/col address mapping schemes.
5. **`Cache`**：Implements a system-level cache to enhance read/write performance.
6. **`Scheduler`**：Implements read/write scheduling and row scheduling policies to leverage the open/row characteristics of memory devices, reducing the execution time of memory access requests.
7. **`Scg`** (SDRAM Command Generator)：Schedules commands and performs timing checks across banks for the DFI interface, ensuring the correctness of memory read/write operations.

## Common Key Functions Across Modules

- **`addTrans()`**：Called by an upstream module to pass signals to this module.
- **`step()`**：Called once per cycle to simulate signal changes within this module during one clock cycle. Calling `step()` sequentially from downstream to upstream modules simulates the pipelining effect of hardware.
- **`all_done()`**：Returns true when this module has no pending requests to process.

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](LICENSE).