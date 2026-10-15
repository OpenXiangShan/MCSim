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

import csv
import sys
from collections import Counter, defaultdict

import matplotlib.pyplot as plt
import numpy as np

NEW_STAGE_COLUMNS = [
    'in_axi_latency', 'in_filter_latency', 'in_addrmap_latency',
    'cache_hit_latency', 'cache_miss_refill_latency', 'cache_to_rob',
    'in_scheduler_latency', 'inout_scg_latency', 'out_axi_latency',
]
LEGACY_STAGE_COLUMNS = NEW_STAGE_COLUMNS


def to_int(value):
    if value is None or value == '' or value == 'None':
        return None
    return int(value)


def read_csv_data(filename):
    try:
        with open(filename, 'r') as csvfile:
            rows = list(csv.DictReader(csvfile))
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f'Error reading file: {e}')
        sys.exit(1)

    if not rows:
        print('Error: CSV has no rows.')
        sys.exit(1)

    stage_columns = [c for c in NEW_STAGE_COLUMNS if c in rows[0]]
    if not stage_columns:
        stage_columns = [c for c in LEGACY_STAGE_COLUMNS if c in rows[0]]
    if not stage_columns:
        print('Error: no known latency stage columns found.')
        sys.exit(1)

    all_latencies = []
    path_latencies = defaultdict(list)
    stage_data = {stage: [] for stage in stage_columns}
    path_stage_data = defaultdict(lambda: {stage: [] for stage in stage_columns})

    for row in rows:
        total = to_int(row.get('all_latency'))
        if total is None:
            continue
        path = row.get('path_type') or 'all'
        all_latencies.append(total)
        path_latencies[path].append(total)
        for stage in stage_columns:
            value = to_int(row.get(stage))
            value = 0 if value is None else value
            stage_data[stage].append(value)
            path_stage_data[path][stage].append(value)

    return all_latencies, path_latencies, stage_data, path_stage_data, stage_columns


def print_averages(all_latencies, path_latencies, stage_data, stage_columns):
    print('Average Latencies:')
    print(f"All Latency: {np.mean(all_latencies):.2f}" if all_latencies else 'All Latency: N/A')
    for path, values in sorted(path_latencies.items()):
        print(f"{path}: count={len(values)}, avg={np.mean(values):.2f}")
    print('\nAverage Stage Latencies:')
    for stage in stage_columns:
        print(f"{stage}: {np.mean(stage_data[stage]):.2f}")


def plot_latency_distribution(path_latencies):
    plt.figure(1)
    for path, values in sorted(path_latencies.items()):
        counts = Counter(values)
        xs = sorted(counts)
        ys = [counts[x] for x in xs]
        plt.step(xs, ys, where='mid', label=path, alpha=0.85)
    plt.xlabel('Latency')
    plt.ylabel('Number of Records')
    plt.title('Latency Distribution by Path')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()


def plot_path_composition(path_stage_data, stage_columns):
    paths = sorted(path_stage_data)
    if not paths:
        return

    plt.figure(2)
    bottom = np.zeros(len(paths))
    x = np.arange(len(paths))
    for stage in stage_columns:
        values = np.array([
            np.mean(path_stage_data[path][stage]) if path_stage_data[path][stage] else 0
            for path in paths
        ])
        if np.all(values == 0):
            continue
        plt.bar(x, values, bottom=bottom, label=stage)
        bottom += values

    plt.xticks(x, paths, rotation=25, ha='right')
    plt.ylabel('Average Latency (cycles)')
    plt.title('Average Latency Composition by Path')
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True, axis='y', alpha=0.3)
    plt.tight_layout()


def main():
    if len(sys.argv) != 2:
        print('Usage: python plot_latency.py <filename>')
        sys.exit(1)

    all_latencies, path_latencies, stage_data, path_stage_data, stage_columns = read_csv_data(sys.argv[1])
    print_averages(all_latencies, path_latencies, stage_data, stage_columns)
    plot_latency_distribution(path_latencies)
    plot_path_composition(path_stage_data, stage_columns)
    plt.show(block=True)


if __name__ == '__main__':
    plt.ioff()
    main()
