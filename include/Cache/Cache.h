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
#include "Cache/MainPipe.h"
#include "Cache/UnreadyQueue.h"
#include "utils/common.h"
#include "utils/FixedQueue.h"
#include "config.h"
#include <array>
#include <vector>
#include <queue>
#include <utility>
#include <assert.h>

class Controller;
class TransactionSchedule;
class AXI2UI;
class MSHR;
class Prefetcher;
class Log;
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
    bool is_hited;   // TODO: set and unset is_hited
    uint64_t last_access_cycle = 0;   // for LRU
    uint64_t tag;
    MappedTransaction initial_transaction;   // for writeback to mem
    bool from_prefetch;   // for statistics
    bool prefetch_accessed;   // for statistics, true if prefetch is accessed (stats modify it)
};

class Cache {
    friend class CacheStatistics;
    friend class MainPipe;
private:
    FixedQueue<MappedTransaction> cmdQueue;
    FixedQueue<uint64_t> prefetchQueue;
    FixedQueue<std::pair<MappedTransaction, TIME_TYPE>> pending_mshr_readback_queue;
    std::queue<std::pair<MappedTransaction, uint64_t>> tmp_urqback_queue;   // per-cache, NOT shared
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
    MainPipe mp;
    UnreadyQueue urq;
    struct ReturnToAddrMapArb {
        // priority: mshr > urq > mpS3
        enum class Source {
            MSHR = 0,
            URQ = 1,
            MP_S3 = 2,
            NONE = 3
        };
        struct Candidate {
            bool valid = false;
            MappedTransaction trans;
            TIME_TYPE ready_time = 0;
            CallerType caller = CallerType::CACHE_HIT;
        };

        std::array<Candidate, 3> candidates;

        static size_t index(Source source) {
            assert(source != Source::NONE);
            return static_cast<size_t>(source);
        }
        bool submit(Source source, MappedTransaction trans, TIME_TYPE ready_time, CallerType caller) {
            Candidate &candidate = candidates[index(source)];
            if (candidate.valid) {
                return false;
            }
            candidate.valid = true;
            candidate.trans = trans;
            candidate.ready_time = ready_time;
            candidate.caller = caller;
            return true;
        }
        Source choose() const {
            for (Source source: {Source::MSHR, Source::URQ, Source::MP_S3}) {
                if (candidates[index(source)].valid) {
                    return source;
                }
            }
            return Source::NONE;
        }
        const Candidate& get(Source source) const {
            return candidates[index(source)];
        }
        void clear(Source source) {
            candidates[index(source)] = Candidate();
        }
        bool empty() const {
            return choose() == Source::NONE;
        }
    } back2am_arb;
    struct ReadbackCandidate {
        bool valid = false;
        bool granted = false;
        MappedTransaction trans;
        TIME_TYPE ready_time = 0;
        CallerType caller = CallerType::CACHE_HIT;
    } readback_candidate;
    bool has_refill_req = false;   // true if there is data from Scg during this cycle
    TIME_TYPE get_cycles();
    bool handle_forward(std::deque<uint64_t>::iterator it);
    bool handle_cmd();
    bool handle_prefetch();
    void mainPipe_arb();
    void writeBus_arb();   // write to CacheArray
    bool prepareReadbackCandidate(MappedTransaction t, CallerType caller, TIME_TYPE ready_time);
    void processPendingMSHRReadback();
    void drainBackToAddrMapArb();
    
public:
    Cache();
    void set_axi2ui(AXI2UI *axi2ui);
    rw_trans_type addTrans(const MappedTransaction &, const MappedTransaction &, rw_trans_type);
    std::tuple<size_t, size_t, size_t> get_tag_index_bank(MappedTransaction t);   // addr -> (tag, index, bank)
    void returnFromScg(MappedTransaction t);
    bool returnDataFromURQ(MappedTransaction t, uint64_t way);
    bool returnDataFromMP(MappedTransaction t, CallerType caller);
    bool hasReadyReadback() const;
    void commitReadyReadback();
    bool addPrefetch(uint64_t addr);
    void step();
    void statistics() const;
    void resetStats();
    bool all_done() const;
    Controller *controller;
    TransactionSchedule *scheduler;
    AXI2UI *axi2ui;
    Log *log = nullptr;
};

#endif   // CACHE_H