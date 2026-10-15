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

# check arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <trace_folder> <result_file>"
    exit 1
fi
echo "Batch trace running" > $2
for file in $1/*; do
    # remove folder name
    file_name=$(basename ${file})
    echo "---------------------------------------------" | tee -a $2
    echo "Running ${file_name}" | tee -a $2
    ./mcsim -d 1 -t ${file} | tee -a $2
done