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
import re
import argparse

def parse_log(log_file):
    """
    解析log文件，返回字典 {id_int: timestamp}
    id_int为整数（去除前导零），timestamp为整数，若无[INFO]行则记为0
    """
    id_map = {}
    id_pattern = re.compile(r"piece(\d+)\.")          # 匹配ID数字部分（字符串形式）
    ts_pattern = re.compile(r"timestamp (\d+)")       # 匹配timestamp

    with open(log_file, 'r') as f:
        lines = iter(f)

        for line in lines:
            line = line.strip()
            if not line:
                continue

            # 处理 Processing 行（包含文件名）
            if line.startswith("Processing trace file:"):
                match = id_pattern.search(line)
                if not match:
                    continue
                id_str = match.group(1)   # 原始字符串，可能含前导零
                id_int = int(id_str)      # 转换为整数用于匹配

                # 读取下一个非空行
                next_line = None
                while True:
                    try:
                        nline = next(lines).strip()
                    except StopIteration:
                        break
                    if nline:
                        next_line = nline
                        break

                # 文件已结束，没有后续信息
                if next_line is None:
                    id_map[id_int] = 0
                    continue

                # 情况1：下一行是 [INFO] 行（包含时间戳）
                if next_line.startswith("[INFO]"):
                    ts_match = ts_pattern.search(next_line)
                    if ts_match:
                        timestamp = int(ts_match.group(1))
                    else:  # 备用方法：取最后一个词（假设时间戳是最后一个数字）
                        timestamp = int(next_line.split()[-1])
                    id_map[id_int] = timestamp

                    # 再读取下一非空行（应为 Conversion 行），忽略其内容
                    while True:
                        try:
                            next(lines)   # 直接跳过
                        except StopIteration:
                            break
                        # 这里不检查内容，因为已知格式正确
                        break

                # 情况2：下一行是 Conversion 行（无 INFO）
                elif "Conversion completed" in next_line:
                    id_map[id_int] = 0

                # 情况3：意外内容（按无 INFO 处理，仍记为0）
                else:
                    id_map[id_int] = 0

            # 其他行忽略
    return id_map


def main():
    # 使用 argparse 解析命令行参数
    parser = argparse.ArgumentParser(
        description="""
        本程序用于从trace处理日志文件中提取每个trace文件的ID与时间戳的映射。
        使用说明：
            python extract_timestamp.py logfile
            1. 指定log文件后，程序从标准输入读取ID序列，每个ID占一行
            2. 输入结束时，请按Ctrl+D（Linux/Mac）或Ctrl+Z+Enter（Windows）
            3. 程序会按输入顺序输出对应的结尾空时间戳，每行一个
        如果未提供logfile，程序将抛出异常。
        ID匹配基于数值，因此 "011090" 和 "11090" 视为相同。
        """
    )
    parser.add_argument('logfile', help='日志文件路径')
    args = parser.parse_args()

    # 解析log，建立ID（整数）到时间戳的映射
    id_map = parse_log(args.logfile)

    # 先收集所有输入的ID
    input_ids = []
    for line in sys.stdin:
        line = line.strip()
        if line:
            input_ids.append(line)

    # 统一处理并输出
    for line in input_ids:
        try:
            id_int = int(line)
        except ValueError:
            print(0)
            continue
        print(id_map.get(id_int, 0))


if __name__ == "__main__":
    main()