## 关于

MCSim是一个针对内存控制器的模拟器，高度参数化，可以模拟各种内存控制器配置，并输出各种性能指标。目前和YuQuan在相同SPEC trace上关键性能指标接近。

## 目录结构

```
.
├── docs/           # 文档目录
├── include/        # 头文件目录
│   ├── config.h    # 配置文件，所有模块共用
│   └── ...
├── Makefile        # Makefile
├── mc_trace/       # trace目录
├── scripts/        # 一些实用脚本
└── src/            # 各模块源码目录
    ├── main.cc     # 程序入口
    └── ...
```

## 使用方法

关于安装、编译和运行，请参考[MCSim使用说明](docs/usage/MCSim_usage.md)

## 文档

更多关于MCSim的说明文档在docs/目录下。

## LICENSE

Copyright © 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences.

Copyright © 2021-2026 Beijing Institute of Open Source Chip

MCSim is licensed under [Mulan PSL v2](LICENSE).