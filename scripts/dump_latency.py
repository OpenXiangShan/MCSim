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

import argparse
import os
import csv
import glob
from typing import List, Dict, Tuple, Optional
import re

class TraceProcessor:
    def __init__(self):
        # Module types and their CSV column formats
        self.module_formats = {
            'axi_in': ['cycle_counter', 'araddr', 'id'],
            'filter_in': ['cycle_counter', 'addr', 'id', 'token'],
            'addrmap_in': ['cycle_counter', 'addr', 'id', 'token'],
            'scheduler_in': ['cycle_counter', 'id', 'token', 'rank', 'bg', 'bank', 'row', 'col'],
            'scg_in': ['cycle_counter', 'id', 'token', 'cmdtype', 'rank', 'bg', 'bank', 'row', 'col'],
            'filter_out': ['cycle_counter', 'id', 'token'],
            'axi_out': ['cycle_counter', 'id', 'token']
        }
        
        # Processing pipeline: each stage's (start_module, end_module)
        self.pipeline_stages = [
            ('axi_in', 'filter_in'),      # in_axi_latency
            ('filter_in', 'addrmap_in'),  # in_filter_latency  
            ('addrmap_in', 'scheduler_in'), # in_addrmap_latency
            ('scheduler_in', 'scg_in'),   # in_scheduler_latency
            ('scg_in', 'filter_out'),        # in_scg_out_filter_latency
            ('filter_out', 'axi_out')     # out_axi_latency
        ]
        
        self.stage_names = [
            'in_axi_latency',
            'in_filter_latency', 
            'in_addrmap_latency',
            'in_scheduler_latency',
            'in_scg_out_filter_latency',
            'out_axi_latency'
        ]
    
    def parse_arguments(self):
        """Parse command line arguments"""
        parser = argparse.ArgumentParser(description='Process trace files to generate latency information')
        parser.add_argument('-in', '--input_dir', required=True,
                          help='Directory containing module_in files')
        parser.add_argument('-out', '--output_dir', required=True,
                          help='Directory containing module_out files')
        parser.add_argument('-o', '--output_file', required=True,
                          help='Output CSV file path')
        parser.add_argument('--intermediate', 
                          help='Optional intermediate file to save timing data')
        
        return parser.parse_args()
    
    def find_trace_files(self, base_dir: str) -> Dict[str, str]:
        """
        Find all trace files in the directory
        Returns: {module_type: file_path}
        """
        files = {}
        
        for module_type in self.module_formats.keys():
            # Normal case: single file
            pattern = os.path.join(base_dir, f"{module_type}_*.txt")
            matching_files = glob.glob(pattern)
            if matching_files:
                files[module_type] = matching_files[0]  # Take first match
        
        return files

    def parse_csv_file(self, file_path: str, expected_format: List[str]) -> List[Dict]:
        """
        Parse CSV file and return list of records
        Skip first line (header) and parse according to expected format
        """
        records = []
        
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
                
                # Skip first line (header)
                for line_num, line in enumerate(lines[1:], 2):  # Start from line 2
                    line = line.strip()
                    if not line:  # Skip empty lines
                        continue
                    
                    parts = line.split(',')
                    if len(parts) < len(expected_format):
                        print(f"Warning: Line {line_num} in {file_path} has insufficient columns")
                        continue
                    
                    # Create record dict based on expected format
                    record = {}
                    for i, field_name in enumerate(expected_format):
                        if i < len(parts):
                            record[field_name] = parts[i].strip()
                    
                    # Convert numeric fields
                    try:
                        # cycle_counter is hexadecimal
                        record['cycle_counter'] = int(record['cycle_counter'], 16)
                        
                        # All other numeric fields are hexadecimal
                        if 'addr' in record:
                            record['addr'] = int(record['addr'], 16)
                        if 'araddr' in record:
                            record['araddr'] = int(record['araddr'], 16)
                        if 'id' in record:
                            record['id'] = int(record['id'], 16)
                        if 'token' in record:
                            record['token'] = int(record['token'], 16)
                        if 'bg' in record:
                            record['bg'] = int(record['bg'], 16)
                        if 'bank' in record:
                            record['bank'] = int(record['bank'], 16)
                        if 'rid' in record:
                            record['rid'] = int(record['rid'], 16)
                        if 'rank' in record:
                            record['rank'] = int(record['rank'], 16)
                        if 'row' in record:
                            record['row'] = int(record['row'], 16)
                        if 'col' in record:
                            record['col'] = int(record['col'], 16)
                        if 'cmdtype' in record:
                            record['cmdtype'] = record['cmdtype']  # Keep as string
                    except ValueError as e:
                        print(f"Warning: Failed to convert numeric field in {file_path} line {line_num}: {e}")
                        continue
                    
                    records.append(record)
                    
        except Exception as e:
            print(f"Error parsing file {file_path}: {e}")
            return []
        
        return records
    
    def build_lookup_tables(self, all_data: Dict[str, any]) -> Dict[str, Dict]:
        """
        Build efficient lookup tables for O(1) access
        Returns lookup tables for each module type
        """
        lookup_tables = {}
        
        # Build lookup table for modules with id (id -> list of records)
        # No sorting needed as files are already in time order
        for module_type in ['filter_in', 'addrmap_in', 'scheduler_in', 'scg_in', 
                            'filter_out', 'axi_out']:
            if module_type in all_data:
                lookup_tables[module_type] = {}
                for record in all_data[module_type]:
                    id = record['id']
                    if id not in lookup_tables[module_type]:
                        lookup_tables[module_type][id] = []
                    lookup_tables[module_type][id].append(record)
        
        # Build lookup table for modules with addresses (for chronological matching)
        # scheduler_in removed as it doesn't have addr field
        for module_type in ['filter_in', 'addrmap_in']:
            if module_type in all_data:
                lookup_tables[f"{module_type}_by_addr"] = {}
                for record in all_data[module_type]:
                    addr = record['addr']
                    if addr not in lookup_tables[f"{module_type}_by_addr"]:
                        lookup_tables[f"{module_type}_by_addr"][addr] = []
                    lookup_tables[f"{module_type}_by_addr"][addr].append(record)
        
        return lookup_tables
    

    
    def generate_intermediate_file(self, input_files: Dict[str, str], output_files: Dict[str, str]) -> str:
        """
        Generate intermediate file with timing information for each request
        Returns path to intermediate file
        """
        # Load all data
        all_data = {}
        
        # Load input files
        for module_type, file_path in input_files.items():
            records = self.parse_csv_file(file_path, self.module_formats[module_type])
            all_data[module_type] = records
            print(f"Loaded {len(records)} records from {module_type}")
        
        # Load output files
        for module_type, file_path in output_files.items():
            records = self.parse_csv_file(file_path, self.module_formats[module_type])
            all_data[module_type] = records
            print(f"Loaded {len(records)} records from {module_type}")
        
        # Build lookup tables for efficient matching
        lookup_tables = self.build_lookup_tables(all_data)
        
        # Track used record indices for each token to maintain order
        used_indices = {}  # {(module_type, token): next_index_to_use}
        used_addr_indices = {}  # {(module, addr): next_index_to_use}
        
        # Generate intermediate records
        intermediate_records = []
        axi_in_records = all_data.get('axi_in', [])
        
        print(f"Processing {len(axi_in_records)} axi_in requests...")
        
        for axi_idx, axi_record in enumerate(axi_in_records):
            if axi_idx % 1000 == 0:
                progress = (axi_idx / len(axi_in_records)) * 100
                print(f"\r  Processed {axi_idx}/{len(axi_in_records)} requests ({progress:.1f}%)", end='', flush=True)
            
            araddr = axi_record['araddr']
            axi_cycle = axi_record['cycle_counter']
            
            intermediate_record = {
                'araddr': f"0x{araddr:x}",
                'in_axi_cycle': axi_cycle,
                'in_filter_cycle': None,
                'in_addrmap_cycle': None,
                'in_scheduler_cycle': None,
                'in_scg_cycle': None,
                'out_filter_cycle': None,
                'out_axi_cycle': None
            }
            
            current_id = None
            current_token = None   # token for check correctness
            
            # Stage 1: axi_in -> filter_in (by address)
            if 'filter_in_by_addr' in lookup_tables and araddr in lookup_tables['filter_in_by_addr']:
                addr_key = ('filter_in', araddr)
                next_idx = used_addr_indices.get(addr_key, 0)
                addr_records = lookup_tables['filter_in_by_addr'][araddr]
                
                if next_idx < len(addr_records):
                    filter_record = addr_records[next_idx]
                    # Since cycle_counter increases, filter should have greater cycle_counter than axi
                    if filter_record['cycle_counter'] >= axi_cycle:
                        intermediate_record['in_filter_cycle'] = filter_record['cycle_counter']
                        current_id = filter_record['id']
                        current_token = filter_record['token']
                        used_addr_indices[addr_key] = next_idx + 1
            
            # Stage 2-10: Use id-based matching with index tracking
            if current_id is not None:
                # Get next index for this id in filter_in (already used above)
                filter_key = ('filter_in', current_id)
                if filter_key not in used_indices:
                    used_indices[filter_key] = 0
                
                # addrmap_in
                if ('addrmap_in' in lookup_tables and current_id in lookup_tables['addrmap_in']):
                    addrmap_key = ('addrmap_in', current_id)
                    next_idx = used_indices.get(addrmap_key, 0)
                    id_records = lookup_tables['addrmap_in'][current_id]
                    
                    if next_idx < len(id_records):
                        record = id_records[next_idx]
                        assert record['token'] == current_token, "addrmap_in token mismatch"
                        intermediate_record['in_addrmap_cycle'] = record['cycle_counter']
                        used_indices[addrmap_key] = next_idx + 1
                
                # scheduler_in
                if ('scheduler_in' in lookup_tables and current_id in lookup_tables['scheduler_in']):
                    scheduler_key = ('scheduler_in', current_id)
                    next_idx = used_indices.get(scheduler_key, 0)
                    id_records = lookup_tables['scheduler_in'][current_id]
                    
                    if next_idx < len(id_records):
                        record = id_records[next_idx]
                        assert record['token'] == current_token, "scheduler_in token mismatch"
                        intermediate_record['in_scheduler_cycle'] = record['cycle_counter']
                        used_indices[scheduler_key] = next_idx + 1
                
                # scg_in
                if ('scg_in' in lookup_tables and current_id in lookup_tables['scg_in']):
                    scg_in_key = ('scg_in', current_id)
                    next_idx = used_indices.get(scg_in_key, 0)
                    id_records = lookup_tables['scg_in'][current_id]
                    
                    if next_idx < len(id_records):
                        record = id_records[next_idx]
                        assert record['token'] == current_token, "scg_in token mismatch"
                        intermediate_record['in_scg_cycle'] = record['cycle_counter']
                        used_indices[scg_in_key] = next_idx + 1
                
                # filter_out
                if ('filter_out' in lookup_tables and current_id in lookup_tables['filter_out']):
                    filter_out_key = ('filter_out', current_id)
                    next_idx = used_indices.get(filter_out_key, 0)
                    id_records = lookup_tables['filter_out'][current_id]
                    
                    if next_idx < len(id_records):
                        record = id_records[next_idx]
                        assert record['token'] == current_token, "filter_out token mismatch"
                        intermediate_record['out_filter_cycle'] = record['cycle_counter']
                        used_indices[filter_out_key] = next_idx + 1

                # axi_out
                if 'axi_out' in lookup_tables and current_id in lookup_tables['axi_out']:
                    axi_out_key = ('axi_out', current_id)
                    next_idx = used_indices.get(axi_out_key, 0)
                    id_records = lookup_tables['axi_out'][current_id]
                    
                    if next_idx < len(id_records):
                        record = id_records[next_idx]
                        assert record['token'] == current_token, "axi_out token mismatch"
                        intermediate_record['out_axi_cycle'] = record['cycle_counter']
                        used_indices[axi_out_key] = next_idx + 1
            
            intermediate_records.append(intermediate_record)
        
        # Print final progress and newline
        print(f"\r  Processed {len(axi_in_records)}/{len(axi_in_records)} requests (100.0%)")
        
        return intermediate_records
    
    def records_match(self, record1: Dict, record2: Dict) -> bool:
        """
        Check if two records represent the same request
        """
        # Try matching by token first
        if 'token' in record1 and 'token' in record2:
            return record1['token'] == record2['token']
        
        # Try matching by address
        addr1 = record1.get('addr', record1.get('araddr'))
        addr2 = record2.get('addr', record2.get('araddr'))
        if addr1 is not None and addr2 is not None:
            return addr1 == addr2
        
        # Fallback: cycle counter proximity (not ideal)
        return abs(record1['cycle_counter'] - record2['cycle_counter']) < 10
    
    def write_intermediate_file(self, intermediate_records: List[Dict], output_file: str) -> str:
        """Write intermediate timing data to file"""
        intermediate_file = output_file.rsplit('.', 1)[0] + '_intermediate.csv'
        
        fieldnames = ['araddr', 'in_axi_cycle', 'in_filter_cycle', 'in_addrmap_cycle', 
                     'in_scheduler_cycle', 'in_scg_cycle', 'out_filter_cycle', 'out_axi_cycle']
        
        try:
            with open(intermediate_file, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader()
                writer.writerows(intermediate_records)
            
            print(f"Intermediate file written to: {intermediate_file}")
            print(f"Generated {len(intermediate_records)} intermediate records")
            
        except Exception as e:
            print(f"Error writing intermediate file: {e}")
        
        return intermediate_file
    
    def generate_output_file(self, intermediate_file: str, output_file: str):
        """Generate final output file from intermediate file"""
        try:
            # Read intermediate file
            intermediate_records = []
            with open(intermediate_file, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    # Convert cycle values to int (or None if empty)
                    for key in row:
                        if key != 'araddr':
                            row[key] = int(row[key]) if row[key] and row[key] != 'None' else None
                    intermediate_records.append(row)
            
            print(f"Read {len(intermediate_records)} records from intermediate file")
            
            # Generate output file
            with open(output_file, 'w', newline='') as f:
                header = ['addr', 'all_latency', 'in_axi_latency', 'in_filter_latency', 
                         'in_addrmap_latency', 'in_scheduler_latency', 
                         'in_scg_out_filter_latency', 'out_axi_latency']
                writer = csv.writer(f)
                writer.writerow(header)
                
                for record in intermediate_records:
                    addr = record['araddr']
                    
                    # Get cycle values
                    cycles = {
                        'in_axi': record['in_axi_cycle'],
                        'in_filter': record['in_filter_cycle'],
                        'in_addrmap': record['in_addrmap_cycle'],
                        'in_scheduler': record['in_scheduler_cycle'],
                        'in_scg': record['in_scg_cycle'],
                        'out_filter': record['out_filter_cycle'],
                        'out_axi': record['out_axi_cycle']
                    }
                    
                    # Calculate latencies (earlier_cycle - later_cycle)
                    latencies = {}
                    
                    # Individual stage latencies
                    latencies['in_axi_latency'] = self.calc_latency(cycles['in_axi'], cycles['in_filter'])
                    latencies['in_filter_latency'] = self.calc_latency(cycles['in_filter'], cycles['in_addrmap'])
                    latencies['in_addrmap_latency'] = self.calc_latency(cycles['in_addrmap'], cycles['in_scheduler'])
                    latencies['in_scheduler_latency'] = self.calc_latency(cycles['in_scheduler'], cycles['in_scg'])
                    latencies['in_scg_out_filter_latency'] = self.calc_latency(cycles['in_scg'], cycles['out_filter'])
                    latencies['out_axi_latency'] = self.calc_latency(cycles['out_filter'], cycles['out_axi'])
                    
                    # Total latency
                    latencies['all_latency'] = self.calc_latency(cycles['in_axi'], cycles['out_axi'])
                    
                    # Write row
                    row = [addr, latencies['all_latency'], latencies['in_axi_latency'], latencies['in_filter_latency'], 
                          latencies['in_addrmap_latency'], latencies['in_scheduler_latency'], 
                          latencies['in_scg_out_filter_latency'], latencies['out_axi_latency']]
                    
                    writer.writerow(row)
            
            print(f"Final output written to: {output_file}")
            
        except Exception as e:
            print(f"Error generating output file: {e}")
    
    def calc_latency(self, start_cycle: Optional[int], end_cycle: Optional[int]) -> Optional[int]:
        """Calculate latency between two cycles"""
        if start_cycle is not None and end_cycle is not None:
            return end_cycle - start_cycle
        return None
    
    def process(self):
        """Main processing function"""
        try:
            args = self.parse_arguments()
            
            print(f"Processing parameters:")
            print(f"  Input directory: {args.input_dir}")
            print(f"  Output directory: {args.output_dir}")
            print(f"  Output file: {args.output_file}")
            
            # Find trace files
            input_files = self.find_trace_files(args.input_dir)
            output_files = self.find_trace_files(args.output_dir)
            
            print(f"\nFound input files:")
            for module, path in input_files.items():
                print(f"  {module}: {path}")
            
            print(f"\nFound output files:")
            for module, path in output_files.items():
                print(f"  {module}: {path}")
            
            # Step 1: Generate intermediate file
            print(f"\nStep 1: Generating intermediate file...")
            intermediate_records = self.generate_intermediate_file(input_files, output_files)
            intermediate_file = self.write_intermediate_file(intermediate_records, args.output_file)
            
            # Step 2: Generate final output file from intermediate file
            print(f"\nStep 2: Generating final output file...")
            self.generate_output_file(intermediate_file, args.output_file)
            
            print("\nProcessing completed!")
            
        except Exception as e:
            print(f"Error: {e}")
            return 1
        
        return 0

def main():
    processor = TraceProcessor()
    return processor.process()

if __name__ == "__main__":
    exit(main())