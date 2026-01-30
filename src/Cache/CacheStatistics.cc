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

#include "Cache/CacheStatistics.h"
#include "Cache/Cache.h"

void CacheStatistics::read_hit(const MappedTransaction &t, CacheLine *cacheline)
{
    assert(t.is_read);

    // only count read_hit caused by non-prefetching cmd
    if (!t.is_prefetch) {
        total_access_c++; total_read_c++;
        
        if (!cacheline) {
            // hit from WCB entry
            return;
        }

        if (cacheline->from_prefetch) {
            prefetch_hit_c++;
            // increase prefetch_correct_c only at first hit
            if (!cacheline->prefetch_accessed) {
                prefetch_correct_c++;
            }
            cacheline->prefetch_accessed = true;
        }
    }
}

void CacheStatistics::read_miss(const MappedTransaction &t)
{
    assert(t.is_read);

    // only count read_miss caused by non-prefetching cmd
    if (!t.is_prefetch) {
        total_access_c++; total_miss_c++; total_read_c++; read_miss_c++;
    }
}

void CacheStatistics::write_hit(const MappedTransaction &t, CacheLine *cacheline)
{
    assert(!t.is_read);
    assert(!t.is_prefetch);

    total_access_c++; total_write_c++;

    if (!cacheline) {
        // hit from WCB entry
        return;
    }
    
    if (cacheline->from_prefetch) {
        prefetch_hit_c++;
        // increase prefetch_correct_c only at first hit
        if (!cacheline->prefetch_accessed) {
            prefetch_correct_c++;
        }
        cacheline->prefetch_accessed = true;
    }
}

void CacheStatistics::write_miss(const MappedTransaction &t)
{
    assert(!t.is_read);
    assert(!t.is_prefetch);

    total_access_c++; total_miss_c++; total_write_c++; write_miss_c++;
}

void CacheStatistics::wcb_full()
{
    block_wcb_full++;
}

void CacheStatistics::mshr_full()
{
    block_mshr_full++;
}

void CacheStatistics::cacheline_not_ready()
{
    block_cacheline_not_ready++;
}

void CacheStatistics::all_cacheline_not_ready()
{
    block_all_cacheline_not_ready++;
}

void CacheStatistics::prefetch()
{
    prefetch_c++;
}

void CacheStatistics::print() const
{
    double hit_rate = (1.0 - static_cast<double>(total_miss_c) / total_access_c) * 100;
    double read_hit_rate = (1.0 - static_cast<double>(read_miss_c) / total_read_c) * 100;
    double write_hit_rate = (1.0 - static_cast<double>(write_miss_c) / total_write_c) * 100;
    std::cout << "================= CACHE STATISTICS ==================" << std::endl;
    std::cout << "| total_access: " << total_access_c << ", total_hit: " << total_access_c - total_miss_c \
              << " (" << hit_rate << "%)" << std::endl;
    std::cout << "| total_read: " << total_read_c << ", read_hit: " << total_read_c - read_miss_c \
              << " (" << read_hit_rate << "%)" << std::endl;
    std::cout << "| total_write: " << total_write_c << ", write_hit: " << total_write_c - write_miss_c \
              << " (" << write_hit_rate << "%)" << std::endl;
    std::cout << "| block_wcb_full: " << block_wcb_full << std::endl;
    std::cout << "| block_mshr_full: " << block_mshr_full << std::endl;
    std::cout << "| block_cacheline_not_ready: " << block_cacheline_not_ready << std::endl;
    std::cout << "| block_all_cacheline_not_ready: " << block_all_cacheline_not_ready << std::endl;

    double prefetch_acc_rate = static_cast<double>(prefetch_correct_c) / prefetch_c * 100;
    double prefetch_coverage = static_cast<double>(prefetch_hit_c) / total_access_c * 100;
    std::cout << "| prefetch_c: " << prefetch_c << std::endl;
    std::cout << "| prefetch_correct_c: " << prefetch_correct_c \
              << " (accuracy: " << prefetch_acc_rate << "%)" << std::endl;
    std::cout << "| prefetch_hit_c: " << prefetch_hit_c \
              << " (coverage: " << prefetch_coverage << "%)" << std::endl;
}