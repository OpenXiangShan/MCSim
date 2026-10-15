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

import subprocess
import threading
from concurrent.futures import ThreadPoolExecutor
import time

USER = "zhuchenguang"
PROCESS = "mcsim_"

def check_server_mcsim_count(server, user=USER):
    """检查单个服务器上运行的特定程序数量"""
    try:
        # 使用pgrep命令统计特定进程数量
        cmd = f"pgrep -f '{PROCESS}' | wc -l"
        result = subprocess.run(
            ['ssh', f'{user}@{server}', cmd],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        if result.returncode == 0:
            count = int(result.stdout.strip())
            return server, count, "success"
        else:
            return server, 0, f"pgrep failed: {result.stderr}"
            
    except subprocess.TimeoutExpired:
        return server, 0, "timeout"
    except Exception as e:
        return server, 0, f"error: {str(e)}"

def monitor_processes(servers, user=USER, interval=5):
    """持续监控所有服务器上的特定进程"""
    print(f"Monitoring {PROCESS} processes on {len(servers)} servers...")
    print("Press Ctrl+C to stop monitoring\n")
    
    try:
        while True:
            start_time = time.time()
            
            # 使用线程池并行检查所有服务器
            with ThreadPoolExecutor(max_workers=20) as executor:
                futures = [executor.submit(check_server_mcsim_count, server, user) for server in servers]
                
                results = []
                for future in futures:
                    results.append(future.result())
            
            # 按服务器IP排序结果
            results.sort(key=lambda x: [int(part) for part in x[0].split('.')])
            
            # 打印结果
            print(f"\n=== {time.strftime('%Y-%m-%d %H:%M:%S')} ===")
            total_count = 0
            for server, count, status in results:
                status_display = f" ({status})" if status != "success" else ""
                print(f"{server}: {count} {PROCESS} processes{status_display}")
                total_count += count
            
            print(f"TOTAL: {total_count} {PROCESS} processes across {len(servers)} servers")
            
            # 计算并等待下一个间隔
            elapsed = time.time() - start_time
            sleep_time = max(0, interval - elapsed)
            time.sleep(sleep_time)
            
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")

def main():
    # 服务器列表 - 与你的主脚本保持一致
    servers = [f'172.28.9.{i}' for i in range(101, 116)]  # 101-115
    
    # 可选：从命令行参数获取服务器列表
    import sys
    if len(sys.argv) > 1:
        if sys.argv[1] == "-h" or sys.argv[1] == "--help":
            print("Usage: python3 monitor_proc_on_server.py [server1 server2 ...]")
            print("If no servers provided, uses default list")
            return
        
        servers = sys.argv[1:]
    
    print(f"Monitoring {len(servers)} servers:")
    for server in servers:
        print(f"  {server}")
    
    # 开始监控
    monitor_processes(servers)

if __name__ == "__main__":
    main()