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
import csv
import glob
import os
from collections import defaultdict
from typing import Dict, List, Optional, Tuple


class TraceProcessor:
    def __init__(self):
        self.module_formats = {
            'axi_in': ['cycle_counter', 'araddr', 'id'],
            'filter_in': ['cycle_counter', 'addr', 'id', 'token'],
            'addrmap_in': ['cycle_counter', 'addr', 'id', 'token'],
            'cache_in': ['cycle_counter', 'addr', 'id', 'token'],
            'scheduler_in': ['cycle_counter', 'id', 'token', 'rank', 'bg', 'bank', 'row', 'col'],
            'scg_in': ['cycle_counter', 'id', 'token', 'cmdtype', 'rank', 'bg', 'bank', 'row', 'col'],
            'cache_refill': ['cycle_counter', 'id', 'token', 'rank', 'bg', 'bank', 'row', 'col'],
            'rob_in': ['cycle_counter', 'id', 'token', 'source'],
            # Backward-compatible fallback for old logs.
            'filter_out': ['cycle_counter', 'id', 'token'],
            'axi_out': ['cycle_counter', 'id', 'token'],
        }
        self.event_fields = [
            'araddr', 'id', 'token', 'path_type',
            'in_axi_cycle', 'in_filter_cycle', 'in_addrmap_cycle', 'in_cache_cycle',
            'in_scheduler_cycle', 'in_scg_cycle', 'cache_refill_cycle',
            'rob_in_cycle', 'rob_source', 'out_axi_cycle',
        ]
        self.fixed_output_fields = ['addr', 'id', 'token', 'path_type', 'all_latency']
        self.latency_output_fields = [
            'in_axi_latency', 'in_filter_latency', 'in_addrmap_latency',
            'cache_hit_latency', 'cache_miss_refill_latency', 'cache_to_rob',
            'in_scheduler_latency', 'inout_scg_latency', 'out_axi_latency',
        ]

    def parse_arguments(self):
        parser = argparse.ArgumentParser(description='Convert module-wise logs to path-aware latency CSV')
        parser.add_argument('-in', '--input_dir', required=True, help='Directory containing module input logs')
        parser.add_argument('-out', '--output_dir', required=True, help='Directory containing module output logs')
        parser.add_argument('-o', '--output_file', required=True, help='Output CSV file path')
        parser.add_argument('--intermediate', help='Optional intermediate file to save timing data')
        return parser.parse_args()

    def find_trace_files(self, base_dir: str) -> Dict[str, str]:
        files = {}
        for module_type in self.module_formats:
            exact = os.path.join(base_dir, f'{module_type}_cycle.txt')
            if os.path.exists(exact):
                files[module_type] = exact
                continue
            matches = glob.glob(os.path.join(base_dir, f'{module_type}_*.txt'))
            if matches:
                files[module_type] = matches[0]
        return files

    def parse_csv_file(self, file_path: str, expected_format: List[str]) -> List[Dict]:
        records = []
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
        except Exception as e:
            print(f'Error parsing file {file_path}: {e}')
            return records

        for line_num, line in enumerate(lines[1:], 2):
            line = line.strip()
            if not line:
                continue
            parts = [p.strip() for p in line.split(',')]
            if len(parts) < len(expected_format):
                print(f'Warning: Line {line_num} in {file_path} has insufficient columns')
                continue
            record = {field: parts[i] for i, field in enumerate(expected_format)}
            try:
                for key, value in list(record.items()):
                    if key == 'source':
                        record[key] = value
                    elif key == 'cmdtype':
                        record[key] = value
                    elif key == 'cycle_counter':
                        record[key] = int(value, 16)
                    else:
                        record[key] = int(value, 16)
            except ValueError as e:
                print(f'Warning: Failed to convert numeric field in {file_path} line {line_num}: {e}')
                continue
            records.append(record)
        return records

    @staticmethod
    def by_id_token(records: List[Dict]) -> Dict[Tuple[int, int], List[Dict]]:
        table = defaultdict(list)
        for record in records:
            if 'id' in record and 'token' in record:
                table[(record['id'], record['token'])].append(record)
        return table

    @staticmethod
    def by_addr(records: List[Dict], addr_field: str = 'addr') -> Dict[int, List[Dict]]:
        table = defaultdict(list)
        for record in records:
            if addr_field in record:
                table[record[addr_field]].append(record)
        return table

    def load_all_data(self, input_files: Dict[str, str], output_files: Dict[str, str]) -> Dict[str, List[Dict]]:
        all_data = {}
        for module_type, file_path in {**input_files, **output_files}.items():
            records = self.parse_csv_file(file_path, self.module_formats[module_type])
            all_data[module_type] = records
            print(f'Loaded {len(records)} records from {module_type}')
        return all_data

    @staticmethod
    def next_from_table(table: Dict, used: Dict, module: str, key,
                        min_cycle: Optional[int] = None,
                        max_cycle: Optional[int] = None) -> Optional[Dict]:
        records = table.get(key, [])
        used_key = (module, key)
        idx = used.get(used_key, 0)
        while idx < len(records) and min_cycle is not None and records[idx]['cycle_counter'] < min_cycle:
            idx += 1
        if idx >= len(records):
            used[used_key] = idx
            return None
        if max_cycle is not None and records[idx]['cycle_counter'] > max_cycle:
            # The next event belongs to a later reuse of the same id/token.
            used[used_key] = idx
            return None
        used[used_key] = idx + 1
        return records[idx]

    @staticmethod
    def infer_path_type(rob_in: Optional[Dict], cache_in: Optional[Dict], scheduler_in: Optional[Dict]) -> str:
        if not rob_in:
            return 'unknown'
        source = rob_in.get('source', 'legacy_filter_out')
        if source == 'cache_hit':
            return 'cache_hit'
        if source == 'cache_miss':
            return 'cache_miss'
        if source == 'scg':
            return 'scg_direct'
        if cache_in:
            return 'cache_unknown'
        if scheduler_in:
            return 'legacy_scg_path'
        return source or 'unknown'

    def generate_intermediate_records(self, all_data: Dict[str, List[Dict]]) -> List[Dict]:
        lookup = {module: self.by_id_token(records) for module, records in all_data.items()}
        filter_by_addr = self.by_addr(all_data.get('filter_in', []), 'addr')
        used = {}
        intermediate = []
        axi_in_records = all_data.get('axi_in', [])

        print(f'Processing {len(axi_in_records)} axi_in requests...')
        for axi_idx, axi in enumerate(axi_in_records):
            if axi_idx % 1000 == 0 and axi_in_records:
                progress = axi_idx / len(axi_in_records) * 100
                print(f'\r  Processed {axi_idx}/{len(axi_in_records)} requests ({progress:.1f}%)', end='', flush=True)

            addr = axi['araddr']
            axi_cycle = axi['cycle_counter']
            record = {field: None for field in self.event_fields}
            record['araddr'] = addr
            record['in_axi_cycle'] = axi_cycle

            filter_in = self.next_from_table(filter_by_addr, used, 'filter_in_by_addr', addr, axi_cycle)
            if not filter_in:
                record['path_type'] = 'unmatched_filter'
                intermediate.append(record)
                continue

            key = (filter_in['id'], filter_in['token'])
            record['id'] = filter_in['id']
            record['token'] = filter_in['token']
            record['in_filter_cycle'] = filter_in['cycle_counter']

            addrmap = self.next_from_table(lookup.get('addrmap_in', {}), used, 'addrmap_in', key, filter_in['cycle_counter'])
            if addrmap:
                record['in_addrmap_cycle'] = addrmap['cycle_counter']

            min_after_addrmap = record['in_addrmap_cycle'] or record['in_filter_cycle']
            cache_in = self.next_from_table(lookup.get('cache_in', {}), used, 'cache_in', key, min_after_addrmap)
            if cache_in:
                record['in_cache_cycle'] = cache_in['cycle_counter']

            rob_in = self.next_from_table(
                lookup.get('rob_in', {}), used, 'rob_in', key, record['in_filter_cycle'])
            if not rob_in and 'filter_out' in lookup:
                rob_in = self.next_from_table(lookup['filter_out'], used, 'filter_out', key, record['in_filter_cycle'])
                if rob_in:
                    rob_in['source'] = 'legacy_filter_out'
            if rob_in:
                record['rob_in_cycle'] = rob_in['cycle_counter']
                record['rob_source'] = rob_in.get('source', 'unknown')

            scheduler = None
            rob_cycle = record['rob_in_cycle']
            rob_source = record['rob_source']
            needs_scg_path = rob_source in (None, 'cache_miss', 'scg', 'legacy_filter_out')
            if needs_scg_path:
                scheduler = self.next_from_table(
                    lookup.get('scheduler_in', {}), used, 'scheduler_in', key,
                    min_after_addrmap, rob_cycle)
                if scheduler:
                    record['in_scheduler_cycle'] = scheduler['cycle_counter']

                scg_min = record['in_scheduler_cycle'] or min_after_addrmap
                scg = self.next_from_table(
                    lookup.get('scg_in', {}), used, 'scg_in', key,
                    scg_min, rob_cycle)
                if scg:
                    record['in_scg_cycle'] = scg['cycle_counter']

                if rob_source in (None, 'cache_miss', 'legacy_filter_out'):
                    refill_min = record['in_scg_cycle'] or record['in_cache_cycle'] or min_after_addrmap
                    refill = self.next_from_table(
                        lookup.get('cache_refill', {}), used, 'cache_refill', key,
                        refill_min, rob_cycle)
                    if refill:
                        record['cache_refill_cycle'] = refill['cycle_counter']

            axi_out_min = record['rob_in_cycle'] or record['in_filter_cycle']
            axi_out = self.next_from_table(lookup.get('axi_out', {}), used, 'axi_out', key, axi_out_min)
            if axi_out:
                record['out_axi_cycle'] = axi_out['cycle_counter']

            record['path_type'] = self.infer_path_type(rob_in, cache_in, scheduler)
            intermediate.append(record)

        print(f'\r  Processed {len(axi_in_records)}/{len(axi_in_records)} requests (100.0%)')
        return intermediate

    def write_intermediate_file(self, intermediate_records: List[Dict], output_file: str, override: Optional[str]) -> str:
        intermediate_file = override or output_file.rsplit('.', 1)[0] + '_intermediate.csv'
        with open(intermediate_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=self.event_fields)
            writer.writeheader()
            writer.writerows(intermediate_records)
        print(f'Intermediate file written to: {intermediate_file}')
        print(f'Generated {len(intermediate_records)} intermediate records')
        return intermediate_file

    @staticmethod
    def calc_latency(start: Optional[int], end: Optional[int]) -> Optional[int]:
        if start is None or end is None:
            return None
        return end - start

    @staticmethod
    def fmt(value: Optional[int]):
        return '' if value is None else value

    def generate_output_file(self, intermediate_records: List[Dict], output_file: str):
        output_rows = []
        for event in intermediate_records:
            c = {
                'axi': event.get('in_axi_cycle'),
                'filter': event.get('in_filter_cycle'),
                'addrmap': event.get('in_addrmap_cycle'),
                'cache': event.get('in_cache_cycle'),
                'scheduler': event.get('in_scheduler_cycle'),
                'scg': event.get('in_scg_cycle'),
                'refill': event.get('cache_refill_cycle'),
                'rob': event.get('rob_in_cycle'),
                'axi_out': event.get('out_axi_cycle'),
            }
            path = event.get('path_type') or 'unknown'
            row = {
                'addr': f"0x{event['araddr']:x}" if event.get('araddr') is not None else '',
                'id': self.fmt(event.get('id')),
                'token': self.fmt(event.get('token')),
                'path_type': path,
                'in_axi_latency': self.calc_latency(c['axi'], c['filter']),
                'in_filter_latency': self.calc_latency(c['filter'], c['addrmap']),
                'in_addrmap_latency': self.calc_latency(c['addrmap'], c['cache'] or c['scheduler']),
                'cache_hit_latency': None,
                'cache_miss_refill_latency': None,
                'cache_to_rob': None,
                'in_scheduler_latency': self.calc_latency(c['scheduler'], c['scg']),
                'inout_scg_latency': self.calc_latency(c['scg'], c['rob']),
                'out_axi_latency': self.calc_latency(c['rob'], c['axi_out']),
                'all_latency': self.calc_latency(c['axi'], c['axi_out']),
            }

            if path == 'cache_hit':
                row['cache_hit_latency'] = self.calc_latency(c['cache'], c['rob'])
            elif path == 'cache_miss':
                row['cache_miss_refill_latency'] = self.calc_latency(c['cache'], c['refill'])
                row['cache_to_rob'] = self.calc_latency(c['refill'], c['rob'])
            elif path.startswith('cache'):
                row['cache_to_rob'] = self.calc_latency(c['cache'], c['rob'])

            output_rows.append(row)

        def has_data(field: str) -> bool:
            return any(row.get(field) is not None for row in output_rows)

        fieldnames = list(self.fixed_output_fields)
        for field in self.latency_output_fields:
            if field.startswith('cache_') and not has_data(field):
                continue
            fieldnames.append(field)

        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for row in output_rows:
                writer.writerow({field: self.fmt(row.get(field)) for field in fieldnames})
        print(f'Final output written to: {output_file}')

    def process(self):
        args = self.parse_arguments()
        print('Processing parameters:')
        print(f'  Input directory: {args.input_dir}')
        print(f'  Output directory: {args.output_dir}')
        print(f'  Output file: {args.output_file}')

        input_files = self.find_trace_files(args.input_dir)
        output_files = self.find_trace_files(args.output_dir)
        print('\nFound input files:')
        for module, path in input_files.items():
            print(f'  {module}: {path}')
        print('\nFound output files:')
        for module, path in output_files.items():
            print(f'  {module}: {path}')

        all_data = self.load_all_data(input_files, output_files)
        intermediate = self.generate_intermediate_records(all_data)
        self.write_intermediate_file(intermediate, args.output_file, args.intermediate)
        self.generate_output_file(intermediate, args.output_file)
        print('\nProcessing completed!')
        return 0


def main():
    return TraceProcessor().process()


if __name__ == '__main__':
    raise SystemExit(main())
