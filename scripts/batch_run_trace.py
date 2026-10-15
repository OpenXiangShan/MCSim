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

"""
分布式trace测试运行器

使用方法：
1. 修改文件开头的配置宏定义以适应你的环境
2. 如需SPEC跑分，可开启 `SPEC_SCORE_MODE` 或传入 `--spec-score`
3. 确保所有服务器都可以通过SSH无密码登录
4. 确保所有服务器上都有相同的项目代码
5. 确保所有服务器都有必要的编译环境

注意事项：
1. 程序会自动备份和恢复config.h和Makefile
2. 可以通过Ctrl+C安全终止程序
3. 输出目录会自动创建，包含时间戳
4. SPEC跑分模式下会自动扫描 `SPEC_TRACE_BASE_DIR` 并覆盖 `TRACE_NAMES`

输出说明：
- elfs/: 编译后的可执行文件目录
- errors/: 错误结果目录
- results/: 测试结果目录
- results/best_configs.log: 最佳配置报告
- results/test_results.xlsx: 所有运行结果
- spec_score_report.csv: SPEC跑分报告（仅SPEC跑分模式生成，按参数组合分别输出）
"""

# ==================== 配置宏定义 ====================
# 服务器配置
SSH_USER = "zhuchenguang"  # SSH用户名
SERVERS_IP = [f'172.28.9.{i}' for i in range(101, 116)]  # 101-115

# 路径配置
TRACE_NAMES = [
               "lbm",
               "leslie3d",
               "libquantum",
               "mcf",
               "milc",
               "omnetpp",
               "soplex",
               "GemsFDTD",
]
ROOT_DIR = "/nfs/home/zhuchenguang/mcsim"  # 项目根目录
def get_trace_dir(trace_name):
    """
    根据trace名称生成对应的trace目录路径
    用户可以根据需要自定义此函数
    """
    return f"/nfs/home/zhuchenguang/mcsim/mc_trace/new/each_pieces/{trace_name}/simple"
    # return "/nfs/home/zhuchenguang/mcsim/mc_trace/new/spec_int"


# SPEC跑分模式配置
SPEC_SCORE_MODE = True       # 算分模式开关；也可通过命令行参数 --spec-score 开启
MC_FREQ_MHZ = 800             # MC频率 (MHz)
CPU_FREQ_MHZ = 3200           # CPU频率 (MHz)
SPEC_TRACE_BASE_DIR = "/nfs/home/zhuchenguang/mcsim/mc_trace/new/each_pieces"  # SPEC模式下自动扫描该目录下的trace
REFTIME_FILE = f"{ROOT_DIR}/scripts/spec06_reftime.json"

# 测试参数范围
PARAM_RANGES = {
    # 'CACHE_EN' : [True, False],
    'CACHE_EN' : [False],
    # 'CACHE_SIZE': [256*1024, 512*1024, 2*1024*1024],  # 256KB, 512KB, 2MB
    # 'CACHE_SIZE': [256*1024],
    # 'PREFETCHER_TYPE': ['NONE', 'STREAM', 'SPATIAL'],  # 关闭、流预取、空间预取
    # 'PREFETCH_GRANULARITY': [2, 4, 8],  # 空间预取器预取粒度
    # 'LRU_EVICT': [True, False],   # LRU替换策略
    # 'row_policy': ['OPEN', 'CLOSE', 'DYNAMIC'],  # 常开行、定时关行、动态关行
    'row_policy': ['OPEN'],
    # 'close_row_time': [0, 100, 200, 400, 800, 1200, 1600, 2000],  # 定时关行时间
    # 'close_row_time': [35],
    # 'rstation_size': [64],  # 读队列大小
    # 'wstation_size': [128],  # 写队列大小
    # 'ref_policy': ['AUTO', 'POSTPONED'],  # 定时刷新、集中刷新
    'ref_policy': ['AUTO'],
    
    # 'TMP': [True, False],  # 临时参数，用于临时测试某一修改的效果
}

# 参数依赖关系
DEPENDENCIES = {
    'CACHE_SIZE': ['CACHE_EN'],  # CACHE_SIZE依赖于CACHE_EN
    'PREFETCHER_TYPE': ['CACHE_EN'],  # PREFETCHER_TYPE依赖于CACHE_EN
    'PREFETCH_GRANULARITY': ['CACHE_EN', 'PREFETCHER_TYPE_SPATIAL'],
    'LRU_EVICT': ['CACHE_EN'],   # LRU_EVICT依赖于CACHE_EN
    'close_row_time': ['row_policy_close'],  # close_row_time依赖于row_policy close
}

# 参数依赖条件（依赖条件没有设置时返回True）
CONDITIONS = {
    'CACHE_EN': lambda params: params.get('CACHE_EN', True) == True,
    'row_policy_close': lambda params: params.get('row_policy', 'CLOSE') == 'CLOSE',  # 只有row_policy为CLOSE时才需要设置close_row_time
    'PREFETCHER_TYPE_SPATIAL': lambda params: params.get('PREFETCHER_TYPE', 'NONE') == 'SPATIAL',  # 新增条件
}
# =====================================================

import os
import sys
import signal
import subprocess
import itertools
import json
import csv
import re
import shutil
import argparse
import math
from datetime import datetime
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
import queue
import time
import zipfile
import xml.etree.ElementTree as ET
from datetime import datetime

class ServerManager:
    def __init__(self, servers):
        self.servers = servers
        self.server_threads = {}  # 存储每个服务器的线程数
        self.server_status = {server: {'tasks': 0, 'last_used': 0} for server in servers}
        self.lock = threading.Lock()
        
        # 获取每个服务器的线程数
        for server in servers:
            try:
                # SSH到服务器获取线程数信息
                ssh = subprocess.Popen(['ssh', f'{SSH_USER}@{server}'],
                                     stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE,
                                     universal_newlines=True)
                stdout, stderr = ssh.communicate(input="lscpu")
                
                if ssh.returncode == 0:
                    # 解析lscpu输出获取线程数
                    threads_per_core = 2
                    cores_per_socket = 64
                    sockets = 2
                    
                    # 记录是否成功解析了每个参数
                    found_threads = False
                    found_cores = False
                    found_sockets = False
                    
                    for line in stdout.split('\n'):
                        if 'Thread(s) per core:' in line:
                            threads_per_core = int(line.split(':')[1].strip())
                            found_threads = True
                        elif '每个核的线程数：' in line:
                            threads_per_core = int(line.split('：')[1].strip())
                            found_threads = True
                        
                        if 'Core(s) per socket:' in line:
                            cores_per_socket = int(line.split(':')[1].strip())
                            found_cores = True
                        elif '每个座的核数：' in line:
                            cores_per_socket = int(line.split('：')[1].strip())
                            found_cores = True
                        
                        if 'Socket(s):' in line:
                            sockets = int(line.split(':')[1].strip())
                            found_sockets = True
                        elif '座：' in line:
                            sockets = int(line.split('：')[1].strip())
                            found_sockets = True
                    
                    if not found_threads:
                        print(f"[INFO] Server {server} use default threads_per_core : {threads_per_core}")
                    if not found_sockets:
                        print(f"[INFO] Server {server} use default cores_per_socket : {cores_per_socket}")
                    if not found_cores:
                        print(f"[INFO] Server {server} use default sockets : {sockets}")
                    
                    self.server_threads[server] = threads_per_core * cores_per_socket * sockets
                    print(f"Server {server}: {sockets} socket(s) x {cores_per_socket} core(s) x {threads_per_core} threads/core = {self.server_threads[server]} total threads")
                else:
                    print(f"Failed to get thread count for {server}, using default")
                    self.server_threads[server] = 8  # 默认值
            except Exception as e:
                print(f"Error getting thread count for {server}: {str(e)}")
                self.server_threads[server] = 8  # 默认值
    
    def get_available_server(self):
        """获取可用的服务器"""
        loop_count = 0
        while True:
            loop_count += 1
            with self.lock:
                # 找到任务数最少的服务器
                available_servers = []
                for server in self.servers:
                    max_tasks = self.server_threads.get(server, 8)  # 使用实际线程数，默认8
                    if self.server_status[server]['tasks'] < max_tasks:
                        available_servers.append((server, 
                                                self.server_status[server]['tasks'],
                                                self.server_status[server]['last_used']))
                
                if available_servers:
                    # 按任务数排序，选择任务数最少的；如果任务数相同，选择最久未使用的
                    server = min(available_servers, key=lambda x: (x[1], x[2]))[0]
                    self.server_status[server]['tasks'] += 1
                    self.server_status[server]['last_used'] = time.time()
                    max_tasks = self.server_threads[server]
                    # print(f"Allocated server: {server} (tasks: {self.server_status[server]['tasks']}/{max_tasks})")
                    return server
                else:
                    if loop_count % 10 == 0:
                        print(f"Waiting for available server (all servers at max capacity)...")
            time.sleep(1)
    
    def release_server(self, server):
        """释放服务器"""
        with self.lock:
            self.server_status[server]['tasks'] = max(0, self.server_status[server]['tasks'] - 1)
    
    @staticmethod
    def check_servers_availability(servers, user):
        available_servers = []
        for server in servers:
            try:
                # 使用ssh命令检查服务器是否可用
                result = subprocess.run(
                    ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", f"{user}@{server}", "exit"],
                    stdout=subprocess.DEVNULL,  # 忽略输出
                    stderr=subprocess.DEVNULL,
                    timeout=10
                )
                if result.returncode == 0:  # 返回码为0表示成功
                    available_servers.append(server)
                else:
                    print(f"Unable to connect to the server via SSH: {server}")
            except Exception as e:
                print(f"Error checking server {server}: {e}")
        return available_servers
    
    @staticmethod
    def cleanup_remote_processes(servers):
        """清理所有服务器上的mcsim_*进程"""
        for server in servers:
            try:
                cleanup_cmd = f"pkill -u '{SSH_USER}' -f 'mcsim_' || true"
                subprocess.run(['ssh', f'{SSH_USER}@{server}', cleanup_cmd],
                             timeout=10, capture_output=True)
                print(f"Cleaned up mcsim processes on {server}")
            except Exception as e:
                print(f"Failed to clean up {server}: {e}")
    

class BackupManager:
    def __init__(self, config_file, makefile):
        self.config_file = config_file
        self.config_backup = config_file + '.backup'
        self.makefile = makefile
        self.makefile_backup = makefile + '.backup'
    
    def backup_config(self):
        """备份原始配置文件"""
        shutil.copy2(self.config_file, self.config_backup)
    
    def restore_config(self):
        """恢复原始配置文件"""
        if os.path.exists(self.config_backup):
            shutil.copy2(self.config_backup, self.config_file)
            os.remove(self.config_backup)
    
    def backup_makefile(self):
        """备份原始Makefile"""
        shutil.copy2(self.makefile, self.makefile_backup)

    def restore_makefile(self):
        """恢复原始Makefile"""
        if os.path.exists(self.makefile_backup):
            shutil.copy2(self.makefile_backup, self.makefile)
            os.remove(self.makefile_backup)
    

class ConfigManager:
    """配置文件管理类"""
    def __init__(self, config_file):
        self.config_file = Path(config_file)
    
    def generate_combinations(self, param_ranges):
        """生成所有参数组合，考虑参数依赖关系"""
        # 定义参数依赖关系
        dependencies = DEPENDENCIES
        
        # 定义依赖条件
        conditions = CONDITIONS
        
        def is_param_valid(param, current_params):
            """检查参数在当前参数组合下是否有效"""
            if param not in dependencies:
                return True
            
            # 检查所有依赖条件，有一条不满足则参数无效
            for dep in dependencies[param]:
                if not conditions[dep](current_params):
                    return False
            return True
        
        def generate_recursive(params_list, current_params, results):
            """递归生成参数组合"""
            if not params_list:
                results.append(current_params.copy())
                return
                
            param, values = params_list[0]
            if is_param_valid(param, current_params):
                for value in values:
                    current_params[param] = value
                    generate_recursive(params_list[1:], current_params, results)
            else:
                # 如果参数无效，只使用第一个值
                current_params[param] = values[0]
                generate_recursive(params_list[1:], current_params, results)
        
        # 生成所有组合
        results = []
        params_list = list(param_ranges.items())
        generate_recursive(params_list, {}, results)
        
        return results
    
    def modify(self, params):
        """修改配置文件"""
        with open(self.config_file, 'r') as f:
            content = f.read()
        
        # 首先设置所有DUMP选项为false
        dump_patterns = {
            'DUMP_TRANS_COUNT': r'static constexpr bool DUMP_TRANS_COUNT = \w+;',
            'DUMP_LATENCY': r'static constexpr bool DUMP_LATENCY = \w+;',
            'DUMP_AS_SCG_TRANS': r'static constexpr bool DUMP_AS_SCG_TRANS = \w+;',
            'DUMP_DFI_RW_TRANS': r'static constexpr bool DUMP_DFI_RW_TRANS = \w+;',
            'DUMP_PREFETCH': r'static constexpr bool DUMP_PREFETCH = \w+;',
            'DUMP_MODULEWISE_LOG': r'static constexpr bool DUMP_MODULEWISE_LOG = \w+;'
        }
        
        for param, pattern in dump_patterns.items():
            content = re.sub(pattern, f'static constexpr bool {param} = false;', content)
        
        # 然后处理其他参数
        for param, value in params.items():
            if param == 'CACHE_EN':
                # 转换Python的True/False为C++的true/false
                cpp_bool = 'true' if value else 'false'
                pattern = r'static constexpr bool CACHE_EN = \w+;'
                content = re.sub(pattern, f'static constexpr bool CACHE_EN = {cpp_bool};', content)
            elif param == 'CACHE_SIZE':
                # 匹配 CACHE_SIZE = 2 * 1024 * 1024 这样的表达式
                pattern = r'static constexpr int CACHE_SIZE = .+;'
                content = re.sub(pattern, f'static constexpr int CACHE_SIZE = {value};', content)
            elif param == 'PREFETCHER_TYPE':
                pattern = r'static constexpr PrefetcherType type = PrefetcherType::\w+;'
                content = re.sub(pattern, f'static constexpr PrefetcherType type = PrefetcherType::{value};', content)
            elif param == 'PREFETCH_GRANULARITY':
                pattern = r'static constexpr int PREFETCH_GRANULARITY = \d+;'
                content = re.sub(pattern, f'static constexpr int PREFETCH_GRANULARITY = {value};', content)
            elif param == 'LRU_EVICT':
                cpp_bool = 'true' if value else 'false'
                pattern = r'static constexpr bool LRU_EVICT = \w+;'
                content = re.sub(pattern, f'static constexpr bool LRU_EVICT = {cpp_bool};', content)
            elif param == 'row_policy':
                pattern = r'static constexpr RowPolicy row_policy = RowPolicy::\w+;'
                content = re.sub(pattern, f'static constexpr RowPolicy row_policy = RowPolicy::{value};', content)
            elif param == 'close_row_time':
                pattern = r'static constexpr int close_row_time = \d+;'
                content = re.sub(pattern, f'static constexpr int close_row_time = {value};', content)
            elif param == 'rstation_size':
                pattern = r'constexpr int rstation_size = \d+;'
                content = re.sub(pattern, f'constexpr int rstation_size = {value};', content)
            elif param == 'wstation_size':
                pattern = r'constexpr int wstation_size = \d+;'
                content = re.sub(pattern, f'constexpr int wstation_size = {value};', content)
            elif param == 'ref_policy':
                pattern = r'static constexpr RefreshPolicy ref_policy = RefreshPolicy::\w+;'
                content = re.sub(pattern, f'static constexpr RefreshPolicy ref_policy = RefreshPolicy::{value};', content)
            
            elif param == 'TMP':
                cpp_bool = 'true' if value else 'false'
                pattern = r'constexpr bool TMP = \w+;'
                content = re.sub(pattern, f'constexpr bool TMP = {cpp_bool};', content)

        with open(self.config_file, 'w') as f:
            f.write(content)


class CompileManager:
    """编译管理类"""
    def __init__(self, root_dir, output_dir, makefile):
        self.root_dir = root_dir
        self.exec_dir = Path(output_dir) / 'elfs'
        self.makefile = makefile
        # 创建可执行文件目录
        self.exec_dir.mkdir(parents=True, exist_ok=True)
        self.compile_error_log = os.path.join(output_dir, 'compile_error.log')
    
    def compile(self, params):
        """编译项目并保存可执行文件"""
        try:
            # 生成唯一的可执行文件名
            param_str = '_'.join(f"{k}_{v}" for k, v in sorted(params.items()))
            exec_name = f"mcsim_{param_str}"
            exec_path = self.exec_dir / exec_name

            # 修改Makefile中的可执行文件名
            subprocess.run(['sed', '-i', 's/^TARGET.*$/TARGET = ' + exec_name + '/', self.makefile], check=True)
            
            # 使用多核编译
            subprocess.run(['make', 'clean'], cwd=self.root_dir,
                                  check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            subprocess.run(['make', '-j', str(os.cpu_count())], cwd=self.root_dir,
                                  check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

            # 移动可执行文件到特定目录
            subprocess.run(['mv', exec_name, str(exec_path)], cwd=self.root_dir, check=True)

            return exec_path
        except subprocess.CalledProcessError as e:
            # 只在出错时显示错误信息
            if e.stderr:
                print(f"Compilation error: {e.stderr.decode()}")
                # 记录编译错误日志
                with open(self.compile_error_log, 'a') as f:
                    f.write(f"Compilation failed for parameters: {params}\n")
                    f.write(e.stderr.decode() + "\n\n")
            return None


class ResultManager:
    """测试结果管理类"""
    def __init__(self, trace_name, output_dir):
        self.trace_name = trace_name
        self.output_dir = Path(output_dir)
        self.results_dir = self.output_dir / 'results' / trace_name
        self.error_dir = self.output_dir / 'errors'
        
        # 创建必要的目录
        for dir_path in [self.results_dir, self.error_dir]:
            dir_path.mkdir(parents=True, exist_ok=True)
    
    def save_result(self, params, trace_file, output):
        """保存测试结果"""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        
        # 判断是否为错误结果
        is_error = output.startswith("Error")
        
        # 获取trace文件的基本名称（含扩展名）
        trace_name = Path(trace_file).name
        
        # 创建trace特定的子目录
        base_dir = self.error_dir if is_error else self.results_dir
        trace_dir = base_dir / trace_name
        trace_dir.mkdir(parents=True, exist_ok=True)
        
        # 生成结果文件名
        param_str = '_'.join(f"{k}_{v}" for k, v in sorted(params.items()))
        result_file = trace_dir / f"result_{timestamp}_{param_str}.json"
        
        # 解析输出中的关键指标
        metrics = {}
        for line in output.split('\n'):
            if 'average_latency:' in line:
                metrics['average_latency'] = line.split(':')[1].strip()
            if 'total_hit:' in line:
                metrics['hit_rate'] = line.split('(')[1].split(')')[0]
            if 'Done' in line:
                metrics['total_cycles'] = line.split()[1]
            if '| prefetch_correct_c:' in line:
                metrics['prefetch_acc'] = line.split('(accuracy: ')[1].split(')')[0]
            if '| prefetch_hit_c:' in line:
                metrics['prefetch_cov'] = line.split('(coverage: ')[1].split(')')[0]

        
        result_data = {
            'timestamp': timestamp,
            'parameters': params,
            'trace': trace_file.name,
            'metrics': metrics,
            'output': output.split('\n')  # 将输出分割成行列表
        }
        
        with open(result_file, 'w') as f:
            json.dump(result_data, f, indent=2)
    
    def generate_excel_report(self):
        """生成包含五个工作表的Excel报告"""
        excel_file = self.output_dir / ('test_results_' + self.trace_name + '.xlsx')

        # 创建Excel文件的基本结构
        with zipfile.ZipFile(excel_file, 'w', zipfile.ZIP_DEFLATED) as zf:
            # 添加必要的文件
            zf.writestr('[Content_Types].xml', '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
    <Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
        <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
        <Default Extension="xml" ContentType="application/xml"/>
        <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
        <Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
        <Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
        <Override PartName="/xl/worksheets/sheet3.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
        <Override PartName="/xl/worksheets/sheet4.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
        <Override PartName="/xl/worksheets/sheet5.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
    </Types>''')

            zf.writestr('_rels/.rels', '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
    <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
        <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
    </Relationships>''')

            zf.writestr('xl/_rels/workbook.xml.rels', '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
    <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
        <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
        <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>
        <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet3.xml"/>
        <Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet4.xml"/>
        <Relationship Id="rId5" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet5.xml"/>
    </Relationships>''')

            # 收集所有数据
            metrics_data = {
                'total_cycles': [],
                'average_latency': [],
                'hit_rate': [],
                'prefetch_acc': [],
                'prefetch_cov': []
            }

            # 获取所有参数名
            all_params = set()
            for trace_dir in self.results_dir.iterdir():
                if not trace_dir.is_dir():
                    continue
                for result_file in trace_dir.glob('result_*.json'):
                    try:
                        with open(result_file, 'r') as f:
                            result = json.load(f)
                            all_params.update(result['parameters'].keys())
                    except:
                        continue
            sorted_params = sorted(all_params)

            # 收集数据
            for trace_dir in self.results_dir.iterdir():
                if not trace_dir.is_dir():
                    continue

                trace_name = trace_dir.name
                for result_file in trace_dir.glob('result_*.json'):
                    try:
                        with open(result_file, 'r') as f:
                            result = json.load(f)

                        params = result['parameters']
                        for metric in metrics_data:
                            if metric in result['metrics']:
                                row = [trace_name]
                                for param in sorted_params:
                                    row.append(str(params.get(param, '')))
                                row.append(str(result['metrics'][metric]))
                                metrics_data[metric].append(row)
                    except:
                        continue
                    
            # 创建工作表
            sheet_names = ['Total Cycles', 'Average Latency', 'Hit Rate', 'Prefetch Accuracy', 'Prefetch Coverage']
            for i, (metric, sheet_name) in enumerate(zip(metrics_data.keys(), sheet_names), 1):
                # 创建工作表XML
                worksheet = ET.Element('worksheet')
                worksheet.set('xmlns', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')

                # 创建表头
                header = ['Trace'] + sorted_params + ['Value']
                sheet_data = ET.SubElement(worksheet, 'sheetData')

                # 添加表头行
                row = ET.SubElement(sheet_data, 'row', {'r': '1'})
                for j, value in enumerate(header, 1):
                    cell = ET.SubElement(row, 'c', {'r': f'{chr(64+j)}1', 't': 'inlineStr'})
                    aaais = ET.SubElement(cell, 'is')
                    t = ET.SubElement(aaais, 't')
                    t.text = value

                # 添加数据行
                for j, data_row in enumerate(metrics_data[metric], 2):
                    row = ET.SubElement(sheet_data, 'row', {'r': str(j)})
                    for k, value in enumerate(data_row, 1):
                        cell = ET.SubElement(row, 'c', {'r': f'{chr(64+k)}{j}', 't': 'inlineStr'})
                        aaais = ET.SubElement(cell, 'is')
                        t = ET.SubElement(aaais, 't')
                        t.text = value

                # 写入工作表文件
                zf.writestr(f'xl/worksheets/sheet{i}.xml', ET.tostring(worksheet, encoding='unicode'))

            # 创建workbook.xml
            workbook = ET.Element('workbook')
            workbook.set('xmlns', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')
            sheets = ET.SubElement(workbook, 'sheets')

            for i, sheet_name in enumerate(sheet_names, 1):
                sheet = ET.SubElement(sheets, 'sheet', {
                    'name': sheet_name,
                    'sheetId': str(i),
                    'r:id': f'rId{i}',
                    'xmlns:r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
                })

            zf.writestr('xl/workbook.xml', ET.tostring(workbook, encoding='unicode'))

        print(f"Excel report saved to: {excel_file}")
        return excel_file

    def analyze_results(self):
        """分析结果并生成最佳配置报告"""
        best_configs = {}

        # 遍历所有trace目录
        for trace_dir in self.results_dir.iterdir():
            if not trace_dir.is_dir():
                continue

            trace_name = trace_dir.name
            all_configs = []  # 存储所有有效配置

            # 遍历该trace下的所有结果文件
            for result_file in trace_dir.glob('result_*.json'):
                try:
                    with open(result_file, 'r') as f:
                        result = json.load(f)

                    # 获取average_latency
                    latency = float(result['metrics'].get('average_latency', float('inf')))

                    # 添加到配置列表
                    all_configs.append({
                        'parameters': result['parameters'],
                        'latency': latency,
                        'hit_rate': result['metrics'].get('hit_rate', 'N/A'),
                        'total_cycles': result['metrics'].get('total_cycles', 'N/A'),
                        'prefetch_acc': result['metrics'].get('prefetch_acc', 'N/A'),
                        'prefetch_cov': result['metrics'].get('prefetch_cov', 'N/A')
                    })

                except (json.JSONDecodeError, ValueError, KeyError) as e:
                    print(f"Warning: Failed to parse {result_file}: {str(e)}")
                    continue
            
            # 按延迟排序，取前几个
            if all_configs:
                best_configs[trace_name] = sorted(all_configs, key=lambda x: x['latency'])[:3]

        # 生成报告文件
        report_file = self.output_dir / 'best_configs.log'
        with open(report_file, 'w') as f:
            f.write("Best Configurations Report\n")
            f.write("=" * 50 + "\n\n")

            for trace_name, configs in sorted(best_configs.items()):
                f.write(f"Trace: {trace_name}\n")
                f.write("-" * 30 + "\n")
                
                # 输出前几个配置
                for i, config in enumerate(configs, 1):
                    f.write(f"\nRank {i}:\n")
                    f.write(f"Average Latency: {config['latency']:.4f}\n")
                    f.write(f"Hit Rate: {config['hit_rate']}\n")
                    f.write(f"Total Cycles: {config['total_cycles']}\n")
                    f.write(f"Prefetch Accuracy: {config['prefetch_acc']}\n")
                    f.write(f"Prefetch Coverage: {config['prefetch_cov']}\n")
                    f.write("Configuration:\n")
                    for param, value in config['parameters'].items():
                        f.write(f"  {param}: {value}\n")
                f.write("\n")

        print(f"Best configurations report saved to: {report_file}")
        
        # 生成Excel报告
        self.generate_excel_report()
        
        return report_file


class SpecScoreCalculator:
    """SPEC跑分计算器"""
    def __init__(self, output_dir, mc_freq_mhz=800, cpu_freq_mhz=3200):
        self.output_dir = Path(output_dir)
        self.mc_freq_mhz = mc_freq_mhz
        self.cpu_freq_mhz = cpu_freq_mhz
        self.cpu_freq_ghz = cpu_freq_mhz / 1000.0
        self.reftime_data = self._load_reftime()

    def _load_reftime(self):
        """加载reftime数据"""
        with open(REFTIME_FILE, 'r') as f:
            return json.load(f)

    @staticmethod
    def _stringify_param_value(value):
        """将参数值转为稳定的字符串，便于分组和CSV输出"""
        return str(value)

    @classmethod
    def make_param_key(cls, params):
        """根据参数字典生成稳定的参数组合键"""
        return tuple((key, cls._stringify_param_value(value)) for key, value in sorted(params.items()))

    @staticmethod
    def _params_from_key(param_key):
        return {key: value for key, value in param_key}

    def _format_params(self, params):
        if not params:
            return "default"
        return ", ".join(f"{key}={self._stringify_param_value(value)}" for key, value in sorted(params.items()))

    def get_trace_programs(self):
        """扫描SPEC trace目录获取所有trace程序"""
        trace_programs = []
        base_path = Path(SPEC_TRACE_BASE_DIR)
        if not base_path.exists():
            return trace_programs

        for entry in sorted(base_path.iterdir()):
            if entry.is_dir():
                # 检查是否有tgz文件或simple目录
                has_tgz = any(entry.glob('*.tgz'))
                has_simple = (entry / 'simple').exists()
                if has_tgz or has_simple:
                    trace_programs.append(entry.name)

        return trace_programs

    def get_reftime_key(self, trace_name):
        """根据trace名称获取reftime键"""
        return trace_name.split('_')[0]

    def load_trace_metadata(self, trace_name):
        """加载trace的元数据"""
        trace_dir = Path(SPEC_TRACE_BASE_DIR) / trace_name

        # 读取process_xxx.json
        json_files = list(trace_dir.glob('process_*_each_pieces.json'))
        if not json_files:
            print(f"[WARNING] No process_xxx.json found for {trace_name}")
            return None

        process_json_path = json_files[0]
        with open(process_json_path, 'r') as f:
            pieces_data = json.load(f)

        # 读取.bb文件行数
        bb_files = list(trace_dir.glob('*.bb'))
        if not bb_files:
            print(f"[WARNING] No .bb file found for {trace_name}")
            return None

        bb_line_count = 0
        with open(bb_files[0], 'r') as f:
            for _ in f:
                bb_line_count += 1

        reftime_key = self.get_reftime_key(trace_name)
        reftime_value = float(self.reftime_data.get(reftime_key, 0))
        if reftime_value == 0:
            print(f"[WARNING] No reftime found for {trace_name} (key: {reftime_key})")

        return {
            'pieces_data': pieces_data,
            'bb_line_count': bb_line_count,
            'reftime_key': reftime_key,
            'reftime_value': reftime_value
        }

    def collect_slice_results_by_config(self, trace_name):
        """按参数组合收集某个trace的所有切片运行结果"""
        results_dir = self.output_dir / 'results' / trace_name
        grouped_results = {}  # {param_key: {'params': params, 'slice_entries': {index: entry}}}

        if not results_dir.exists():
            return grouped_results

        for slice_dir in results_dir.iterdir():
            if not slice_dir.is_dir():
                continue

            # 从目录名提取index
            # e.g., astar_biglakes_piece007408.2th.simple.new -> index=2
            match = re.search(r'\.(\d+)th\.simple\.new$', slice_dir.name)
            if not match:
                continue

            index = int(match.group(1))

            # 读取该切片在每个参数组合下最新的result_*.json
            result_files = sorted(slice_dir.glob('result_*.json'))
            if not result_files:
                continue

            for result_file in result_files:
                try:
                    with open(result_file, 'r') as f:
                        result_data = json.load(f)

                    params = result_data.get('parameters', {})
                    if not isinstance(params, dict):
                        params = {}

                    total_cycles = int(result_data.get('metrics', {}).get('total_cycles', 0))
                    param_key = self.make_param_key(params)
                    config_data = grouped_results.setdefault(param_key, {
                        'params': params,
                        'slice_entries': {}
                    })

                    mtime = result_file.stat().st_mtime
                    old_entry = config_data['slice_entries'].get(index)
                    if old_entry is None or mtime >= old_entry['mtime']:
                        config_data['slice_entries'][index] = {
                            'total_cycles': total_cycles,
                            'mtime': mtime
                        }
                except (json.JSONDecodeError, ValueError, KeyError, TypeError):
                    continue

        return {
            param_key: {
                'params': data['params'],
                'slice_cycles': {
                    index: entry['total_cycles']
                    for index, entry in data['slice_entries'].items()
                }
            }
            for param_key, data in grouped_results.items()
        }

    def collect_slice_results(self, trace_name):
        """收集某个trace的所有切片运行结果（兼容旧的单配置调用）"""
        grouped_results = self.collect_slice_results_by_config(trace_name)
        if not grouped_results:
            return {}

        # 旧接口只能返回一组结果；保留最新出现的参数组合以兼容外部调用。
        latest_key = sorted(grouped_results.keys())[-1]
        return grouped_results[latest_key]['slice_cycles']

    def _calculate_trace_score_from_cycles(self, trace_name, metadata, slice_cycles, params=None):
        """根据给定切片周期计算单个trace的分数"""
        pieces_data = metadata['pieces_data']
        bb_line_count = metadata['bb_line_count']
        reftime_value = metadata['reftime_value']

        # 计算MC运行周期
        mc_cycles = 0
        for piece in pieces_data:
            index = piece['index']
            weight = piece['weight']
            empty_timestamp = piece.get('empty_timestamp', 0)

            total_cycles = slice_cycles.get(index, 0)
            cpu_mc_freq_ratio = self.cpu_freq_mhz / self.mc_freq_mhz

            # (运行周期 + empty_timestamp/4/cpu_mc_freq_ratio) * weight * bb_line_count
            piece_cycles = (total_cycles + empty_timestamp / 4.0 / cpu_mc_freq_ratio) * weight * bb_line_count
            mc_cycles += piece_cycles

        # 运行时间(秒) = MC运行周期 / (MC频率_MHz * 10^6)
        execution_time = mc_cycles / (self.mc_freq_mhz * 1e6)

        # 分数 = reftime / 运行时间
        if execution_time > 0:
            score = reftime_value / execution_time
        else:
            score = 0

        # 每GHz分数
        score_per_ghz = score / self.cpu_freq_ghz if self.cpu_freq_ghz > 0 else 0

        result = {
            'trace_name': trace_name,
            'reftime_key': metadata['reftime_key'],
            'mc_cycles': mc_cycles,
            'execution_time': execution_time,
            'score': score,
            'score_per_ghz': score_per_ghz,
            'reftime': reftime_value,
            'bb_line_count': bb_line_count
        }
        if params is not None:
            result['params'] = params
        return result

    def calculate_trace_score(self, trace_name):
        """计算单个trace的分数（兼容旧的单配置调用）"""
        metadata = self.load_trace_metadata(trace_name)
        if not metadata:
            return None

        slice_cycles = self.collect_slice_results(trace_name)
        return self._calculate_trace_score_from_cycles(trace_name, metadata, slice_cycles)

    def calculate_trace_scores_by_config(self, trace_name):
        """计算单个trace在每个参数组合下的分数"""
        metadata = self.load_trace_metadata(trace_name)
        if not metadata:
            return {}

        grouped_slice_results = self.collect_slice_results_by_config(trace_name)
        trace_scores = {}
        for param_key, data in sorted(grouped_slice_results.items()):
            trace_scores[param_key] = self._calculate_trace_score_from_cycles(
                trace_name,
                metadata,
                data['slice_cycles'],
                data['params']
            )
        return trace_scores

    def _merge_trace_scores(self, trace_scores):
        """将同一参数组合下的trace分数按reftime_key合并"""
        # 按reftime_key合并
        merged_scores = {}
        for ts in trace_scores:
            key = ts['reftime_key']
            if key not in merged_scores:
                merged_scores[key] = {
                    'traces': [],
                    'mc_cycles': 0,
                    'reftime': ts['reftime']
                }
            merged_scores[key]['traces'].append(ts['trace_name'])
            merged_scores[key]['mc_cycles'] += ts['mc_cycles']

        # 计算合并后的分数
        final_scores = []
        for key, data in sorted(merged_scores.items()):
            execution_time = data['mc_cycles'] / (self.mc_freq_mhz * 1e6)
            score = data['reftime'] / execution_time if execution_time > 0 else 0
            score_per_ghz = score / self.cpu_freq_ghz if self.cpu_freq_ghz > 0 else 0

            final_scores.append({
                'program': key,
                'traces': ', '.join(data['traces']),
                'mc_cycles': data['mc_cycles'],
                'execution_time': execution_time,
                'score': score,
                'score_per_ghz': score_per_ghz,
                'reftime': data['reftime']
            })

        # 计算整体几何平均
        if final_scores:
            valid_scores = [fs['score'] for fs in final_scores if fs['score'] > 0]
            valid_score_per_ghz = [fs['score_per_ghz'] for fs in final_scores if fs['score_per_ghz'] > 0]
            if valid_scores:
                log_sum = sum(math.log(s) for s in valid_scores)
                geomean = math.exp(log_sum / len(valid_scores))
            else:
                geomean = 0

            if valid_score_per_ghz:
                log_sum_per_ghz = sum(math.log(s) for s in valid_score_per_ghz)
                geomean_per_ghz = math.exp(log_sum_per_ghz / len(valid_score_per_ghz))
            else:
                geomean_per_ghz = 0
        else:
            geomean = 0
            geomean_per_ghz = 0

        return final_scores, geomean, geomean_per_ghz

    def calculate_all_scores(self):
        """计算所有trace在每个参数组合下的分数"""
        trace_programs = self.get_trace_programs()

        trace_scores_by_config = {}
        params_by_config = {}
        for trace_name in trace_programs:
            config_scores = self.calculate_trace_scores_by_config(trace_name)
            for param_key, score_data in config_scores.items():
                trace_scores_by_config.setdefault(param_key, []).append(score_data)
                params_by_config.setdefault(param_key, score_data.get('params', self._params_from_key(param_key)))

        score_reports = []
        for param_key in sorted(trace_scores_by_config.keys()):
            final_scores, geomean, geomean_per_ghz = self._merge_trace_scores(trace_scores_by_config[param_key])
            score_reports.append({
                'param_key': param_key,
                'params': params_by_config.get(param_key, self._params_from_key(param_key)),
                'final_scores': final_scores,
                'geomean': geomean,
                'geomean_per_ghz': geomean_per_ghz
            })

        return score_reports

    def print_and_save_report(self, score_reports, filename='spec_score_report.csv'):
        """打印并保存按参数组合分组的SPEC跑分报告"""
        report_path = self.output_dir / filename
        show_traces = False  # 控制是否显示Traces列

        named_reports = [
            (f"config_{index:03d}", report)
            for index, report in enumerate(score_reports, 1)
        ]

        if show_traces:
            # 屏幕输出（含Traces列）
            print("\n" + "=" * 100)
            print("SPEC 2006 Score Report")
            print("=" * 100)
            for config_name, report in named_reports:
                print(f"\n[{config_name}] {self._format_params(report['params'])}")
                print(f"{'Program':<15} {'Traces':<40} {'MC_Cycles':>15} {'Time(s)':>12} {'Score':>10} {'Score/GHz':>10}")
                print("-" * 100)

                for fs in report['final_scores']:
                    print(f"{fs['program']:<15} {fs['traces']:<40} {fs['mc_cycles']:>15.0f} {fs['execution_time']:>12.4f} {fs['score']:>10.2f} {fs['score_per_ghz']:>10.2f}")

                print("-" * 100)
                print(f"{'Overall GeoMean':<15} {'':<40} {'':>15} {'':>12} {report['geomean']:>10.2f} {report['geomean_per_ghz']:>10.2f}")
            print("=" * 100)
        else:
            # 屏幕输出（不含Traces列）
            print("\n" + "=" * 80)
            print("SPEC 2006 Score Report")
            print("=" * 80)
            for config_name, report in named_reports:
                print(f"\n[{config_name}] {self._format_params(report['params'])}")
                print(f"{'Program':<15} {'MC_Cycles':>15} {'Time(s)':>12} {'Score':>10} {'Score/GHz':>10}")
                print("-" * 80)

                for fs in report['final_scores']:
                    print(f"{fs['program']:<15} {fs['mc_cycles']:>15.0f} {fs['execution_time']:>12.4f} {fs['score']:>10.2f} {fs['score_per_ghz']:>10.2f}")

                print("-" * 80)
                print(f"{'Overall GeoMean':<15} {'':>15} {'':>12} {report['geomean']:>10.2f} {report['geomean_per_ghz']:>10.2f}")
            print("=" * 80)

        param_names = sorted({
            param_name
            for _, report in named_reports
            for param_name in report['params'].keys()
        })

        base_header = ['Config'] + [f"Param_{param_name}" for param_name in param_names]
        if show_traces:
            header = base_header + ['Program', 'Traces', 'MC_Cycles', 'Execution_Time(s)', 'Score', 'Score_Per_GHz', 'Reftime']
        else:
            header = base_header + ['Program', 'MC_Cycles', 'Execution_Time(s)', 'Score', 'Score_Per_GHz', 'Reftime']

        with open(report_path, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(header)
            for config_name, report in named_reports:
                param_values = [self._stringify_param_value(report['params'].get(param_name, '')) for param_name in param_names]
                for fs in report['final_scores']:
                    if show_traces:
                        row = [config_name] + param_values + [
                            fs['program'],
                            fs['traces'],
                            f"{fs['mc_cycles']:.0f}",
                            f"{fs['execution_time']:.6f}",
                            f"{fs['score']:.4f}",
                            f"{fs['score_per_ghz']:.4f}",
                            fs['reftime']
                        ]
                    else:
                        row = [config_name] + param_values + [
                            fs['program'],
                            f"{fs['mc_cycles']:.0f}",
                            f"{fs['execution_time']:.6f}",
                            f"{fs['score']:.4f}",
                            f"{fs['score_per_ghz']:.4f}",
                            fs['reftime']
                        ]
                    writer.writerow(row)

                if show_traces:
                    writer.writerow([config_name] + param_values + [
                        'Overall_GeoMean', '', '', '',
                        f"{report['geomean']:.4f}",
                        f"{report['geomean_per_ghz']:.4f}",
                        ''
                    ])
                else:
                    writer.writerow([config_name] + param_values + [
                        'Overall_GeoMean', '', '',
                        f"{report['geomean']:.4f}",
                        f"{report['geomean_per_ghz']:.4f}",
                        ''
                    ])

        print(f"\nCSV report saved to: {report_path}")


class TestRunner:
    def __init__(self, root_dir, trace_name, trace_dir, output_dir, config_file, servers):
        self.trace_name = trace_name
        self.trace_dir = Path(trace_dir)
        self.root_dir = Path(root_dir)
        self.server_manager = ServerManager(servers)
        self.backup_manager = BackupManager(config_file, root_dir + "/Makefile")
        self.config_manager = ConfigManager(config_file)
        self.compile_manager = CompileManager(root_dir, output_dir, root_dir + "/Makefile")
        self.result_manager = ResultManager(trace_name, output_dir)
    
    def run_test_on_server(self, server, params, trace_file, exec_path):
        """在指定服务器上运行测试"""
        try:
            # SSH到服务器并执行命令
            ssh = subprocess.Popen(['ssh', f'{SSH_USER}@{server}'],
                                 stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,
                                 universal_newlines=True)
            
            # 构建远程命令
            remote_cmd = f"""
cd {self.root_dir}
{exec_path} -d 1 -t {trace_file}
"""
            
            # 发送命令并获取输出
            stdout, stderr = ssh.communicate(input=remote_cmd)
            
            if ssh.returncode != 0:
                return f"Error on server {server}: {stderr}"
            
            return stdout
            
        except Exception as e:
            return f"Error connecting to server {server}: {str(e)}"
        finally:
            self.server_manager.release_server(server)
    
    def run_tests(self, param_ranges):
        """运行所有测试"""
        # 备份原始配置文件
        self.backup_manager.backup_config()
        self.backup_manager.backup_makefile()
        
        try:
            # 生成所有参数组合
            combinations = self.config_manager.generate_combinations(param_ranges)
            total_compiles = len(combinations)
            completed_compiles = 0
            
            # 获取所有trace文件
            trace_files = []
            for path in self.trace_dir.glob('*'):
                if path.is_file():  # 只保留文件，跳过目录
                    trace_files.append(path)
            total_tasks = len(combinations) * len(trace_files)
            submitted_tasks = 0
            
            # 使用线程池并行执行
            # with ThreadPoolExecutor(max_workers=len(self.server_manager.servers)) as executor:
            with ThreadPoolExecutor() as executor:
                futures = {}
                
                # 启动结果收集线程
                all_tasks_submitted = False
                
                def collect_results():
                    completed_count = 0
                    while True:
                        # 如果所有任务都已提交且futures为空，说明所有任务都已完成
                        if all_tasks_submitted and not futures:
                            break
                        # 如果已完成数量等于总任务数，说明所有任务都已完成
                        if completed_count >= total_tasks:
                            break
                            
                        if not futures:
                            time.sleep(0.1)
                            continue
                            
                        try:
                            for future in as_completed(futures, timeout=0.1):
                                params, trace_file = futures[future]
                                try:
                                    output = future.result()
                                    self.result_manager.save_result(params, trace_file, output)
                                    completed_count += 1
                                    print(f"[INFO] Completed ({completed_count}/{total_tasks}): {trace_file.name} with params {params}")
                                except Exception as e:
                                    print(f"Error running test: {str(e)}")
                                finally:
                                    del futures[future]
                        except:
                            pass
                
                collector_thread = threading.Thread(target=collect_results)
                collector_thread.start()
                
                try:
                # 对每个参数组合
                    for params in combinations:
                        # 修改配置文件
                        self.config_manager.modify(params)

                        # 编译
                        print(f"[{completed_compiles + 1}/{total_compiles}] Compiling configuration:")
                        print(f"  Parameters: {', '.join(f'{k}={v}' for k, v in params.items())}")
                        exec_path = self.compile_manager.compile(params)
                        if exec_path:
                            completed_compiles += 1

                            # 对每个trace文件创建任务
                            for trace_file in trace_files:
                                # 等待可用服务器
                                server = self.server_manager.get_available_server()

                                # 提交任务
                                print(f"Submitting task ({submitted_tasks + 1}/{total_tasks}): {trace_file.name} with params {params} \n  to server {server}")
                                future = executor.submit(self.run_test_on_server, server, params, trace_file, exec_path)
                                futures[future] = (params, trace_file)
                                submitted_tasks += 1
                    
                    # 标记所有任务已提交
                    all_tasks_submitted = True
                    # 等待结果收集完成
                    collector_thread.join()
                except:
                    all_tasks_submitted = True  # 确保异常时也能退出
                    collector_thread.join()
                    raise
        
        finally:
            # 恢复原始配置文件和Makefile
            self.backup_manager.restore_config()
            self.backup_manager.restore_makefile()



def main():
    parser = argparse.ArgumentParser(description='MCSim Distributed Trace Runner')
    parser.add_argument('--spec-score', action='store_true', help='Enable SPEC score calculation mode')
    args = parser.parse_args()

    # 在测试开始前检查服务器可用性
    global SERVERS_IP, TRACE_NAMES
    print(f"getting available servers...")
    SERVERS_IP = ServerManager.check_servers_availability(SERVERS_IP, SSH_USER)
    print(f"available server list: {SERVERS_IP}")

    # 服务器列表
    servers = SERVERS_IP

    # 配置参数范围
    param_ranges = PARAM_RANGES

    # 设置路径
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    output_dir = ROOT_DIR + '/run.logs' + f'/{timestamp}'
    config_file = ROOT_DIR + '/include/config.h'  # 配置文件路径

    # 信号处理
    def signal_handler(signum, frame):
        print(f"\nReceived signal {signum}, cleaning up remote processes...")
        ServerManager.cleanup_remote_processes(servers)
        print("Cleanup completed. Exiting.")
        sys.exit(1)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # 算分模式：通过宏或命令行参数开启，并自动扫描SPEC trace目录覆盖TRACE_NAMES
    spec_score_enabled = args.spec_score or SPEC_SCORE_MODE
    if spec_score_enabled:
        print("[INFO] SPEC Score Mode enabled")
        calculator = SpecScoreCalculator(output_dir, MC_FREQ_MHZ, CPU_FREQ_MHZ)
        auto_traces = calculator.get_trace_programs()
        print(f"[INFO] Found {len(auto_traces)} trace programs: {auto_traces}")
        TRACE_NAMES = auto_traces

    # 对TRACE_NAMES中的每一个元素,创建测试运行器
    for trace_name in TRACE_NAMES:
        TRACE_DIR = get_trace_dir(trace_name)
        runner = TestRunner(ROOT_DIR, trace_name, TRACE_DIR, output_dir, config_file, servers)

        try:
            runner.run_tests(param_ranges)
            # 分析结果并生成报告
            runner.result_manager.analyze_results()
        except Exception as e:
            print(f"Error during test execution: {str(e)}")
            raise

    # 算分模式下，所有trace跑完后计算并输出分数
    if spec_score_enabled:
        print("\n[INFO] Calculating SPEC scores...")
        calculator = SpecScoreCalculator(output_dir, MC_FREQ_MHZ, CPU_FREQ_MHZ)
        score_reports = calculator.calculate_all_scores()
        calculator.print_and_save_report(score_reports)

if __name__ == '__main__':
    main()
