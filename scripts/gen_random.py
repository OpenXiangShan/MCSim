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

import random

def gen_random():
	for i in range(50):
		print(f"{0} R {hex(random.randint(0, 67108863) * 0x40)}")
	for i in range(2000000 - 50):
		print(f"{random.randint(16, 32)} R {hex(random.randint(0, 67108863) * 0x40)}")


# first sequential read, then read randomly
def gen_random_repeat(base, upper, lines):
    assert (base <= upper) & (base % 0x40 == 0) & (upper % 0x40 == 0)
    j = int(lines - (upper - base) / 0x40)
    assert j >= 0
    
    for i in range(base, upper, 0x40):
        print(f"0 R {hex(i)}")
    for _ in range(j):
        print(f"0 R {hex(random.randint(base/0x40, upper/0x40 - 1) * 0x40)}")

# random sequence, but one cacheline appear once and cover addr range
def gen_random_once(base, upper):
    assert (base <= upper) & (base % 0x40 == 0) & (upper % 0x40 == 0)
    cacheline_list = list(range(base, upper, 64))
    random.shuffle(cacheline_list)
    for i in cacheline_list:
        request = random.choices(['R', 'W'], [0.9, 0.1])
        print(f"0 {request[0]} {hex(i)}")



if __name__ == "__main__":
    # gen_random()
    # gen_random_repeat(0, 256*1024, 2000000)
    gen_random_once(0, 2000000*64)