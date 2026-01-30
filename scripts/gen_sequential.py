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

def gen_sequential(base, lines):
    i = base
    for _ in range(lines):
        print(f"0 R {hex(i)}")
        i += 0x40

def gen_sequential_repeat(base, upper, lines):
    assert (base <= upper) & (base % 0x40 == 0) & (upper % 0x40 == 0)
    i = base
    for _ in range(lines):
        if i >= upper:
            i = (i - base) % (upper - base) + base
        print(f"0 R {hex(i)}")
        i += 0x40
    

if __name__ == "__main__":
    # gen_sequential(0, 1000000)
    gen_sequential_repeat(0, 256*1024, 2000000)