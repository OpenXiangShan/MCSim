#!/usr/bin/env python3
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

import sys
import argparse

def load_files(simpoints_file, weights_file):
    """加载两个文件并构建映射关系"""
    # 读取simpoints文件，建立ID到序号的映射
    id_to_simpoint = {}
    try:
        with open(simpoints_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 2:
                    simpoint_id = parts[0]
                    simpoint_num = parts[1]
                    id_to_simpoint[simpoint_id] = simpoint_num
    except FileNotFoundError:
        print(f"错误: 找不到simpoints文件: {simpoints_file}", file=sys.stderr)
        sys.exit(1)
    
    # 读取weights文件，建立序号到权重的映射
    weight_to_num = {}
    try:
        with open(weights_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 2:
                    weight = float(parts[0])
                    weight_num = parts[1]
                    weight_to_num[weight_num] = weight
    except FileNotFoundError:
        print(f"错误: 找不到weights文件: {weights_file}", file=sys.stderr)
        sys.exit(1)
    
    return id_to_simpoint, weight_to_num

def process_ids(input_ids, simpoints_file, weights_file):
    """处理输入的ID序列，返回对应的权重序列"""
    id_to_simpoint, weight_to_num = load_files(simpoints_file, weights_file)
    
    results = []
    for simpoint_id in input_ids:
        # 获取对应的序号
        if simpoint_id not in id_to_simpoint:
            print(f"警告: ID {simpoint_id} 在simpoints文件中未找到", file=sys.stderr)
            results.append(None)
            continue
        
        simpoint_num = id_to_simpoint[simpoint_id]
        
        # 通过序号获取权重
        if simpoint_num not in weight_to_num:
            print(f"警告: 序号 {simpoint_num} 在weights文件中未找到", file=sys.stderr)
            results.append(None)
            continue
        
        weight = weight_to_num[simpoint_num]
        results.append(weight)
    
    return results

def main():
    # 创建参数解析器
    parser = argparse.ArgumentParser(
        description='''
        处理simpoints和weights文件，输出对应的权重。
        
        使用说明：
        1. 程序从标准输入读取ID序列，每个ID占一行
        2. 输入结束时，请按Ctrl+D（Linux/Mac）或Ctrl+Z+Enter（Windows）
        3. 程序会按输入顺序输出对应的权重值，每行一个
        '''
    )
    parser.add_argument('-s', '--simpoints', required=True, 
                       metavar='simpoints_path',
                       help='simpoints文件路径')
    parser.add_argument('-w', '--weights', required=True,
                       metavar='weights_path',
                       help='weights文件路径')
    
    # 解析命令行参数
    args = parser.parse_args()
    
    # 从标准输入读取ID序列
    input_ids = []
    for line in sys.stdin:
        line = line.strip()
        if line:  # 忽略空行
            input_ids.append(line)
    
    # 处理ID序列并获取权重
    weights = process_ids(input_ids, args.simpoints, args.weights)
    
    # 输出权重，每行一个
    for weight in weights:
        if weight is not None:
            print(weight)
        else:
            # 对于找不到的ID，输出空行或特定标记
            print("")

if __name__ == "__main__":
    main()
