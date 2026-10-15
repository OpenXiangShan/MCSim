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

#ifndef STREAMPREFETCHER_V2_H
#define STREAMPREFETCHER_V2_H
#include <array>
#include "config.h"
#include "top/Transaction.h"
#include "utils/FixedQueue.h"
#include "Cache/Prefetcher.h"

class StreamPrefetcher_V2 : public Prefetcher{
private:
    static constexpr const int QueueDepth = 64;
    static constexpr const int count_width = 8;
    static constexpr const int NumStride = 2;
    // Global History Buffer Entry
    class GhbEntry {
    public:
        bool valid;
        uint64_t addr;
        struct GhbEntryStridePair {
            uint64_t stride;   // stride for this stream
            uint64_t count;   // count of how many times this stride has been seen
        } strides[cfg.GHB_NUM_ENTRIES];
    };
    std::array<GhbEntry, cfg.GHB_NUM_ENTRIES> ghb;
    std::array<uint64_t, cfg.GHB_NUM_ENTRIES> fetching_history;
    FixedQueue<uint64_t> outq;
    void update_ghb(uint64_t block_addr);
    void update_outq();
    void update_fetching_history(uint64_t block_addr);
    void do_prefetch();
    bool is_repeated(uint64_t block_addr);

public:
    StreamPrefetcher_V2();
    void step() override;
    bool addTrans(const MappedTransaction &t) override;
};
#endif   // STREAMPREFETCHER_V2_H