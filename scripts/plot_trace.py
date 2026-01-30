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

import matplotlib.pyplot as plt
import re
import sys

def plot_address_trace(trace_file_path):
    # 存储读取到的地址和对应的访问序号
    addresses = []
    indices = []
    
    # 正则表达式匹配trace文件中的行，支持带0x前缀的十六进制地址
    pattern = re.compile(r'^\d+\s+[RW]\s+(0x)?([0-9a-fA-F]+)$')
    
    # 读取文件并提取地址
    with open(trace_file_path, 'r') as file:
        for index, line in enumerate(file):
            line = line.strip()
            if not line:
                continue
                
            match = pattern.match(line)
            if match:
                # 提取地址部分（可能包含0x前缀）
                addr_str = match.group(2)  # 获取十六进制数字部分
                # 将十六进制地址转换为整数
                addr = int(addr_str, 16)
                # if (addr >> 21) & 1 == 0:
                #     assert(False)
                addresses.append(addr)
                indices.append(index)
            else:
                print(f"Warning: Cannot parse line {index}: {line}")
    
    # 创建图表
    plt.figure(figsize=(12, 8))
    plt.scatter(indices, addresses, s=1, alpha=0.5)
    plt.xlabel('Access Order (Operation Index)')
    plt.ylabel('Memory Address')
    plt.title('Memory Access Pattern')
    plt.grid(True, alpha=0.3)
    
    # 如果地址范围很大，使用科学计数法
    if addresses and max(addresses) > 1e6:
        plt.ticklabel_format(style='scientific', axis='y', scilimits=(0,0))
    
    plt.tight_layout()
    plt.show()


def plot_latency_trace(latency_file_path):
    latencyes = []
    indices = []
    pattern = re.compile(r'^(\d+)')
    
    # 读取文件并提取地址
    with open(latency_file_path, 'r') as file:
        for index, line in enumerate(file):
            if not line:
                continue
                
            match = pattern.match(line)
            if match:
                latency_str = match.group(1)
                latency = int(latency_str, 10)
                latencyes.append(latency)
                indices.append(index)
            else:
                print(f"Warning: Cannot parse line {index}: {line}")
    
    # 创建图表
    plt.figure(figsize=(12, 8))
    plt.scatter(indices, latencyes, s=1, alpha=0.5)
    plt.xlabel('Access Order (Operation Index)')
    plt.ylabel('Latency')
    plt.title('Latency Pattern')
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()


# (latency, num)
def plot_latency_distribution(latency_file_path):
    # 使用字典统计每个延迟值的出现次数
    latency_count = {}
    pattern = re.compile(r'^(\d+)')
    
    # 读取文件并统计延迟值
    with open(latency_file_path, 'r') as file:
        for line in file:
            if not line.strip():
                continue
                
            match = pattern.match(line)
            if match:
                latency_str = match.group(1)
                latency = int(latency_str, 10)
                # 统计每个延迟值的出现次数
                latency_count[latency] = latency_count.get(latency, 0) + 1
            else:
                print(f"Warning: Cannot parse line: {line}")
    
    # 如果没有有效数据，直接返回
    if not latency_count:
        print("No valid latency data found.")
        return
    
    # 提取延迟值和对应的计数
    latencies = list(latency_count.keys())
    counts = list(latency_count.values())
    
    # 创建图表
    plt.figure(figsize=(12, 8))
    plt.bar(latencies, counts, alpha=0.7)
    plt.xlabel('Latency')
    plt.ylabel('Number of Records')
    plt.title('Latency Distribution')
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python plot_trace.py <trace_file_path>")
        sys.exit(1)
    trace_file_path = sys.argv[1]
    # plot_address_trace(trace_file_path)
    # plot_latency_trace(trace_file_path)
    plot_latency_distribution(trace_file_path)