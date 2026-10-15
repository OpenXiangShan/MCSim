#!/bin/bash
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

# usage: ./cook_each_pieces.sh <*.tgz>

set -e
if [ $# -ne 1 ]; then
    echo "usage: ./cook_each_pieces.sh <*.tgz>"
    exit 1
fi

TGZ_FILE="$1"

if [ ! -f "$TGZ_FILE" ]; then
    echo "error: file '$TGZ_FILE' not found"
    exit 1
fi

TGZ_DIR=$(dirname "$TGZ_FILE")
TGZ_NAME=$(basename "$TGZ_FILE")
LOG_FILE="${TGZ_DIR}/process_${TGZ_NAME%.tgz}.log"

exec 2>&1
{
    cd "$TGZ_DIR"
    
    # 1. unzip tgz
    tar -xzvf "$TGZ_NAME"
    mkdir -p bin simple
    EXTRACTED_DIR=$(tar -tzf "$TGZ_NAME" | head -1 | cut -f1 -d"/")
    mv "$EXTRACTED_DIR"/* .
    rmdir "$EXTRACTED_DIR" 2>/dev/null || true
    mv *.bin bin/ 2>/dev/null || true
    
    # 2. cook each bin file
    echo ""
    cd bin
    for bin_file in *.bin; do
        if [ -f "$bin_file" ]; then
            python3 ~/mcsim/scripts/cook_bin_trace.py "$bin_file" 2>&1
        fi
    done
    mv *.new ../simple/ 2>/dev/null || true
    
    # 3. get information
    cd ../simple
    echo ""
    echo "each R/W counts:"
    find . -type f \( -name "*.simple.new" -o -name "*.new" \) | xargs -I {} sh -c "wc -l {}" | awk '{print $1}'
    echo ""
    echo "each timestamps:"
    find . -type f \( -name "*.simple.new" -o -name "*.new" \) | xargs -I {} sh -c "tail -n 1 {}" | awk '{print $3}'
} | tee -a "$LOG_FILE"

# 4. generate structured data file
cd "$TGZ_DIR"
python3 - << 'PYTHON_SCRIPT'
import json
import os
import re
import struct
import glob

def get_last_empty_timestamp(bin_file):
    """Read the last trace from bin file, return empty timestamp if last trace is empty."""
    with open(bin_file, 'rb') as f:
        f.seek(0, 2)
        size = f.tell()
        if size < 8:
            return None
        f.seek(-8, 2)
        data = f.read(8)
        trace_value = struct.unpack('<Q', data)[0]
        empty = (trace_value >> 63) & 1
        if empty == 1:
            timestamp_delta = trace_value & ((1 << 63) - 1)
            return timestamp_delta << 2
        return None

# read simpoints: simpoint_id index
simpoints = {}
with open('simpoints', 'r') as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 2:
            simpoints[int(parts[1])] = parts[0]

# read weights: weight index
weights = {}
with open('weights', 'r') as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 2:
            weights[int(parts[1])] = float(parts[0])

# infer trace_name from first bin file
trace_name = ""
bin_files = glob.glob('bin/*.bin')
if bin_files:
    basename = os.path.basename(bin_files[0])
    trace_name = basename.split('_piece')[0]

# scan bin and simple directories to build index -> path mappings
# filename format: {trace_name}_piece{simpoint_id_padded}.{index}th.bin
bin_map = {}
for f in glob.glob('bin/*.bin'):
    m = re.search(r'\.(\d+)th\.bin$', os.path.basename(f))
    if m:
        bin_map[int(m.group(1))] = f

simple_map = {}
for f in glob.glob('simple/*.simple.new'):
    m = re.search(r'\.(\d+)th\.simple\.new$', os.path.basename(f))
    if m:
        simple_map[int(m.group(1))] = f

result = []
for idx in sorted(simpoints.keys()):
    simpoint_id = simpoints[idx]
    weight = weights.get(idx, 0.0)

    bin_file = bin_map.get(idx)
    simple_file = simple_map.get(idx)

    empty_ts = None
    rw_count = 0
    timestamp = 0

    # get empty_timestamp from bin file
    if bin_file and os.path.exists(bin_file):
        empty_ts = get_last_empty_timestamp(bin_file)

    # get rw_count and timestamp from simple file
    if simple_file and os.path.exists(simple_file):
        with open(simple_file, 'r') as f:
            lines = f.readlines()
            rw_count = len(lines)
            if lines:
                parts = lines[-1].strip().split()
                if len(parts) >= 3:
                    try:
                        timestamp = int(parts[2])
                    except ValueError:
                        timestamp = 0
    else:
        # no corresponding simple file: use default values
        empty_ts = 10000000
        rw_count = 0
        timestamp = 0

    result.append({
        'index': idx,
        'simpoint_id': simpoint_id,
        'weight': weight,
        'empty_timestamp': empty_ts if empty_ts is not None else 0,
        'rw_count': rw_count,
        'timestamp': timestamp
    })

data_file = f'process_{trace_name}_each_pieces.json'
with open(data_file, 'w') as f:
    json.dump(result, f, indent=2)

print(f"Structured data file generated: {data_file}")
PYTHON_SCRIPT
