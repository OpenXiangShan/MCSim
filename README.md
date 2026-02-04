## About

MCSim is a simulator for memory controllers. It is highly parameterized and capable of simulating various memory controller configurations, outputting a range of performance metrics. Its key performance indicators are currently close to those of YuQuan on the same SPEC traces.

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

## Usage

For installation, compilation, and running instructions, please refer to the [MCSim Usage Guide](docs/en/usage/MCSim_usage.md).

## Documentation

More documentation about MCSim can be found in the docs/ directory.

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](LICENSE).