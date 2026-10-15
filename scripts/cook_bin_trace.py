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

import struct
import sys

def get_bits(trace_value, msb, lsb):
    assert msb >= lsb, "MSB must be greater than or equal to LSB"
    return (trace_value >> lsb) & ((1 << (msb - lsb + 1)) - 1)

def process_trace(input_file, output_file):
    """
    Process binary trace file and convert to .simple format.
    Bin Trace Format (MSB to LSB):
        empty(1) | id(14) | dependency(8) | timestamp_delta(6) | consistency(3) | R/W(1) | addr(31)
    Output .simple Format:
        id, dependency, timestamp, R/W, addr
    
    Args:
        input_file: Input .bin file path
        output_file: Output .simple file path
    """
    try:
        with open(input_file, 'rb') as bin_file, open(output_file, 'w') as simple_file:
            current_timestamp = 0
            last_trace = { "empty" : False, "timestamp_delta" : 0 }
            
            while True:
                # Read 64 bits (8 bytes) for each trace
                data = bin_file.read(8)
                if not data:
                    if last_trace["empty"]:
                        print(f'[INFO] Last trace empty with timestamp {last_trace["timestamp_delta"]}')
                    break
                
                # Unpack 64-bit value (little endian)
                trace_value = struct.unpack('<Q', data)[0]
                
                # Extract fields according to bit positions (from MSB to LSB)
                empty = get_bits(trace_value, 63, 63)
                trace_id = get_bits(trace_value, 62, 49)
                dependency = get_bits(trace_value, 48, 41)
                timestamp_delta = get_bits(trace_value, 40, 35)
                consistency = get_bits(trace_value, 34, 32)   # ignore
                rw_flag = get_bits(trace_value, 31, 31)
                address = get_bits(trace_value, 30, 0)
                
                # For empty trace, timestamp is bit 62-0
                if empty == 1:
                    timestamp_delta = get_bits(trace_value, 62, 0)
                
                # Apply left shift to get real timestamp delta and real address
                timestamp_delta = timestamp_delta << 2
                real_address = address << 5
                # real_address = address
                
                # Accumulate timestamp (always add delta, even for empty packets)
                current_timestamp += timestamp_delta
                
                # Skip output for empty packets, but timestamp delta is already accumulated
                rw_char = "R" if rw_flag == 1 else "W"
                if empty == 0:
                    # Format: timestamp, id, dependency, R/W, addr
                    line = f"{trace_id} {dependency} {current_timestamp} {rw_char} 0x{real_address:x}\n"
                    simple_file.write(line)
                # else:
                    # For empty trace, print its timestamp
                    # print(f"empty trace with timestamp {timestamp_delta}")
                
                # Record last trace
                last_trace["empty"] = empty
                last_trace["timestamp_delta"] = timestamp_delta
                
    except FileNotFoundError:
        print(f"Error: Input file '{input_file}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"Error processing files: {e}")
        sys.exit(1)

def get_output_filename(input_file):
    if input_file.endswith('.bin'):
        return input_file[:-4] + '.simple.new'
    else:
        return input_file + '.simple.new'

def main():
    if len(sys.argv) != 2:
        print("Usage: python cook_bin_trace.py <input.bin>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = get_output_filename(input_file)
    
    print(f"Processing trace file: {input_file}")
    process_trace(input_file, output_file)
    print(f"Conversion completed. Output file: {output_file}")

if __name__ == "__main__":
    main()