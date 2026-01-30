/***************************************************************************************
* Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
* Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
*
* MCSim is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#ifndef CACHESTATISTICS_H
#define CACHESTATISTICS_H
#include <iostream>
#include "utils/common.h"
#include "top/Transaction.h"

class CacheLine;
class CacheStatistics {
private:
    uint64_t total_access_c, total_miss_c, \
        total_read_c, read_miss_c, \
        total_write_c, write_miss_c;
    uint64_t block_wcb_full, block_mshr_full, block_cacheline_not_ready, block_all_cacheline_not_ready;
    uint64_t prefetch_hit_c;   // hit and block from prefetch count
    uint64_t prefetch_c, prefetch_correct_c;   // prefetch count, prefetch correct count
public:
    CacheStatistics(): total_access_c(0), total_miss_c(0), \
        total_read_c(0), read_miss_c(0), \
        total_write_c(0), write_miss_c(0), \
        block_wcb_full(0), block_mshr_full(0), block_cacheline_not_ready(0), block_all_cacheline_not_ready(0), \
        prefetch_hit_c(0), prefetch_c(0), prefetch_correct_c(0) {};
    void read_hit(const MappedTransaction &, CacheLine *);
    void read_miss(const MappedTransaction &);
    void write_hit(const MappedTransaction &, CacheLine *);
    void write_miss(const MappedTransaction &);
    void wcb_full();
    void mshr_full();
    void cacheline_not_ready();   // "match but block" or "not match but all ways not ready"
    void all_cacheline_not_ready();   // "not match but all ways not ready"
    void prefetch();
    void print() const;
};

#endif   // CACHESTATISTICS_H