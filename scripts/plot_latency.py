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
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
import sys

def read_csv_date(filename):
    """Read CSV data and return all_latencies and module_data"""
    all_latencies = []
    module_data = defaultdict(list)
    module_names = [
        'in_axi_latency', 'in_filter_latency', 'in_addrmap_latency',
        'in_scheduler_latency', 'in_scg_out_filter_latency', 'out_axi_latency'
    ]
    
    try:
        with open(filename, 'r') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                # Convert values to integers
                all_latency_val = int(row['all_latency'])
                all_latencies.append(all_latency_val)
                
                # Store module latencies for this record
                for module in module_names:
                    module_data[module].append(int(row[module]))
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
        
    return all_latencies, module_data, module_names

def calculate_averages(all_latencies, module_data, module_names):
    """Calculate average latencies"""
    avg_all_latency = np.mean(all_latencies)
    avg_module_latencies = {module: np.mean(module_data[module]) for module in module_names}
    
    return avg_all_latency, avg_module_latencies

def plot_latency_distribution(all_latencies):
    """Plot latency distribution using plt.bar with a dictionary of counts"""
    # Count occurrences of each latency value
    latency_counts = {}
    for latency in all_latencies:
        if latency in latency_counts:
            latency_counts[latency] += 1
        else:
            latency_counts[latency] = 1
            
    # Sort latencies for plotting
    sorted_latencies = sorted(latency_counts.keys())
    counts = [latency_counts[lat] for lat in sorted_latencies]
    
    # Create first figure: the bar plot
    plt.figure(1)
    plt.bar(sorted_latencies, counts, alpha=0.7)
    plt.xlabel('Latency')
    plt.ylabel('Number of Records')
    plt.title('Latency Distribution')
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
def plot_module_composition(all_latencies, module_data, module_names):
    """Plot module latency composition (Figure 2)"""
    # Group records by all_latency value
    latency_groups = defaultdict(list)
    for i, latency in enumerate(all_latencies):
        latency_groups[latency].append(i)
    
    # Calculate average module percentages for each latency value
    latencies = sorted(latency_groups.keys())
    module_percentages = {module: [] for module in module_names}
    
    for latency in latencies:
        indices = latency_groups[latency]
        for module in module_names:
            # Calculate average percentage for this module at this latency
            avg_module_val = np.mean([module_data[module][i] for i in indices])
            module_percentages[module].append(avg_module_val / latency * 100)
            
    # Create second figure: the stacked bar plot
    plt.figure(2)
    bottom = np.zeros(len(latencies))
    
    for module in module_names:
        plt.bar(latencies, module_percentages[module], bottom=bottom, label=module)
        bottom += np.array(module_percentages[module])
        
    plt.xlabel('Latency')
    plt.ylabel('Percentage of Latency (%)')
    plt.title('Composition of Latency by Module')
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
def main():
    # Check if filename is provided as argument
    if len(sys.argv) != 2:
        print('Usage: python plot_latency.py <filename>')
        sys.exit(1)
    
    filename = sys.argv[1]
    
    # Read data from CSV
    all_latencies, module_data, module_names = read_csv_date(filename)
    
    # Calculate averages
    avg_all_latency, avg_module_latencies = calculate_averages(all_latencies, module_data, module_names)
    
    # Print average latencies
    print("Average Latencies:")
    print(f"All Latency: {avg_all_latency:.2f}")
    for module, avg in avg_module_latencies.items():
        print(f"{module}: {avg:.2f}")
        
    # Plot figures
    plot_latency_distribution(all_latencies)
    plot_module_composition(all_latencies, module_data, module_names)
    
    # Show both figures
    # plt.show()
    plt.show(block=True)
    
if __name__ == "__main__":
    plt.ioff()
    main()
