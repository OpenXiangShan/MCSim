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

#ifndef _PARTIAL_PREFETCHER_H_
#define _PARTIAL_PREFETCHER_H_
#include "config.h"
#include "top/Transaction.h"
#include "utils/FixedQueue.h"
#include "Cache/Prefetcher.h"

class SpatialPrefetcher : public Prefetcher {
private:
    static constexpr const int QueueDepth = 64;
    FixedQueue<uint64_t> input_queue;
    FixedQueue<uint64_t> output_queue;
    uint64_t get_base_block_addr(uint64_t block_addr);
    std::vector<uint64_t> get_adj_cachelines(uint64_t block_addr);
    void do_prefetch();
    bool input_contains(uint64_t addr);
    bool output_contains(uint64_t block_addr);

public:
    SpatialPrefetcher() :
        input_queue(QueueDepth), output_queue(QueueDepth * cfg.PREFETCH_GRANULARITY) {}
    void step() override;
    bool addTrans(const MappedTransaction &t) override;
};

#endif   // _PARTIAL_PREFETCHER_H_