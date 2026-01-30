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

#ifndef PREFETCHER_H
#define PREFETCHER_H
#include <array>
#include "config.h"
#include "top/Transaction.h"
#include "utils/FixedQueue.h"

class Cache;
// TODO: Baiyang use uint8_t stride which may cause accuracy of prefetch down
class Prefetcher {
private:
    // reference config
    static constexpr const \
        CacheConfig::PrefetcherConfig &cfg = CacheConfig::pfc_cfg;
    static constexpr const int QueueDepth = 64;
    // Global History Buffer Entry
    class GhbEntry {
    public:
        bool valid;
        uint64_t addr;
        struct GhbEntryStridePair {
            bool valid;
            uint64_t stride;   // stride for this stream
            uint64_t count;   // count of how many times this stride has been seen
        } strides[cfg.MAX_STREAMS];
    };
    // sequential logic signals
    std::array<GhbEntry, cfg.GHB_NUM_ENTRIES> ghb;
    enum State {
        IDLE,   // s1
        CAL_STRIDE,   // s2
        UPDATE_STRIDE,   // s3
        PREFETCH   // s4
    } state;
    FixedQueue<uint64_t> s1_input_queue;
    FixedQueue<uint64_t> *s4_output_queue[cfg.MAX_STREAMS];
    struct Stage1 {
        bool miss_valid = false;
    } s1;
    struct Stage2 {
        int stride_count = 0;   // how many valid strides have been calculated
    } s2;
    struct Stage3 {
        int index;
        bool fini;
    } s3;
    struct Stage4 {
        bool fini, need_prefetch;
        int index;
    } s4;
    void s1_init();
    void s2_init();
    void s2_to_s3_init();
    void s1_to_s4_init();
    void update_ghb();
    void cal_stride();
    void update_stride();
    void do_prefetch();
    bool is_prefetched(uint64_t addr);
    void s4_rr_arb();

public:
    Prefetcher();
    Cache *cache;
    void step();
    bool addTrans(const MappedTransaction &t);
};
#endif   // PREFETCHER_H