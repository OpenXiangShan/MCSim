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

#ifndef CACHE_H
#define CACHE_H
#include "Cache/MSHR.h"
#include "Cache/CacheStatistics.h"
#include "utils/common.h"
#include "utils/FixedQueue.h"
#include "config.h"
#include <vector>

class TransactionSchedule;
class AXI2UI;
class MSHR;
class Prefetcher;
enum class CacheLineQueryResult {
    MISS,   // tag not match || valid is false
    HIT,   // tag match && valid is true && ready is true
    BLOCK,  // 1. tag match && valid is true && ready is false
            // 2. tag not match && all ways valid is true && all ways ready is false
    MSHR_FULL,
    WCB_FULL
};
class CacheLine {
public:
    CacheLine(): valid(false), dirty(false), \
        tag(0), from_prefetch(false) {};
    CacheLine(bool valid, bool dirty, uint64_t tag): valid(valid), dirty(dirty), \
        tag(tag), from_prefetch(false) {};
    bool valid;
    bool dirty;
    bool ready;
    uint64_t tag;
    MappedTransaction initial_transaction;   // for writeback to mem
    bool from_prefetch;   // for statistics
    bool prefetch_accessed;   // for statistics, true if prefetch is accessed (stats modify it)
};

class Cache {
    friend class CacheStatistics;
private:
    FixedQueue<MappedTransaction> cmdQueue;
    FixedQueue<uint64_t> prefetchQueue;
    size_t cache_size;
    size_t cacheline_size;
    size_t ways;
    size_t num_sets;
    size_t num_banks;   // divide cache by banks
    std::vector<std::vector<std::vector<CacheLine>>> CacheArray;   // (num_banks, num_sets, ways)
    MSHR *mshr;   // MissStatusHoldingRegisters, for miss read
    FixedQueue<MappedTransaction> *wcb;   // WriteCombineBuffer, for miss write
    Prefetcher *prefetcher;
    CacheStatistics stats;
    std::pair<bool, CacheLineQueryResult> addCacheLine(const MappedTransaction &t);
    bool addMissedCacheLine(size_t bank, size_t index, uint64_t tag, bool dirty, bool ready, MappedTransaction t);
    size_t evictCacheLine(size_t bank, size_t index);   // return (size_t)(-1) if wcb full
    size_t allocateCacheLine(size_t bank, size_t index);   // return (size_t)(-1) if failed
    std::tuple<size_t, size_t, size_t> get_tag_index_bank(MappedTransaction t);   // addr -> (tag, index, bank)
public:
    Cache();
    void set_axi2ui(AXI2UI *axi2ui);
    rw_trans_type addTrans(const MappedTransaction &, const MappedTransaction &, rw_trans_type);
    void returnFromScg(MappedTransaction t);
    bool addPrefetch(uint64_t addr);
    void step();
    void statistics() const;
    TransactionSchedule *scheduler;
    AXI2UI *axi2ui;
};

#endif   // CACHE_H