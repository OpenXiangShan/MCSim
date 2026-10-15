#
# Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
# Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
#
# MCSim is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# See the Mulan PSL v2 for more details.

# 从旧格式trace生成新格式trace
import sys
import random

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python gen_new_trace.py <old_trace>")
        sys.exit(1)

    old_trace_file = sys.argv[1]
    new_trace_file = old_trace_file + '.new'
    
    # 在每一行的前面加上随机的id和随机的dependency
    with open(old_trace_file, 'r') as f_old, open(new_trace_file, 'w') as f_new:
        for line in f_old:
            line = line.strip()
            if line:
                # 生成随机的id和随机的dependency, id: 0-127, dependency: 0-63
                id = str(random.randint(0, 127))
                dependency = str(random.randint(0, 63))
                f_new.write(f"{id} {dependency} {line}\n")
    print(f"New trace generated: {new_trace_file}")
    print("Done!")
    sys.exit(0)
