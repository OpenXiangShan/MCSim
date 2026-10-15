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

#include "Cache/StreamPrefetcher_V2.h"
#include "Cache/Cache.h"
#include <assert.h>

StreamPrefetcher_V2::StreamPrefetcher_V2()
    : outq(QueueDepth)
{
    // init ghb
    for (auto &entry : ghb) {
        entry.valid = false;
        entry.addr = 0;
        for (auto &stride : entry.strides) {
            stride.stride = 0;
            stride.count = 0;
        }
    }
    // init fetching_history
    for (auto &entry : fetching_history) {
        entry = 0;
    }
}

// calculate stride and update ghb
void StreamPrefetcher_V2::update_ghb(uint64_t block_addr)
{
    for (int i = ghb.size() - 1; i > 0; i--) {
        ghb[i] = ghb[i - 1];
    }
    ghb[0].valid = true;
    ghb[0].addr = block_addr;
    for (int i = 0; i < cfg.GHB_NUM_ENTRIES; i++) {
        if (ghb[i].valid) {
            // TODO: What if stride in an opposite direction?
            uint64_t stride = block_addr - ghb[i].addr;
            bool matches = false;
            uint64_t count = 0;
            for (int j = 1; j < cfg.GHB_NUM_ENTRIES; j++) {
                if (stride == ghb[i].strides[j].stride) {
                    // NOTICE: Baiyang may suppose only one stride in ghb[i].strides is matched
                    // if (matches) [[unlikely]] {
                    //     std::cerr << "Multiple matches stride " << stride << std::endl;
                    //     std::abort();
                    // }
                    matches = true;
                    count |= ghb[i].strides[j].count;
                }
            }

            ghb[0].strides[i].stride = stride;
            if (matches) {
                ghb[0].strides[i].count = ((count + 1) >= (1 << count_width)) ? (count >> 1) : (count + 1);
            } else {
                ghb[0].strides[i].count = 1;
            }
        }
    }
}

bool StreamPrefetcher_V2::is_repeated(uint64_t block_addr)
{
    bool addr_exists = false;
    for (auto &fetched : fetching_history) {
        if (fetched == block_addr) {
            addr_exists = true;
            break;
        }
    }
    return addr_exists;
}

void StreamPrefetcher_V2::update_outq()
{
    if (!(ghb[0].valid && !outq.full())) {
        // just drop the req when outq is full
        return;
    }

    auto max_it = std::max_element(
        std::begin(ghb[0].strides), std::end(ghb[0].strides),
        [](const auto &a, const auto &b) {
            return a.count < b.count;
        }
    );
    int max_index = std::distance(std::begin(ghb[0].strides), max_it);

    if (!(ghb[0].strides[max_index].count > cfg.PREFETCH_THRESHOLD)) {
        return;
    }

    uint64_t outq_enq_bits = ghb[0].addr + ghb[0].strides[max_index].stride * NumStride;
    if (is_repeated(outq_enq_bits)) {
        return;
    }

    outq.push(outq_enq_bits);
}

void StreamPrefetcher_V2::update_fetching_history(uint64_t block_addr)
{
    for (int i = cfg.GHB_NUM_ENTRIES - 1; i > 0; i--) {
        fetching_history[i] = fetching_history[i - 1];
    }
    fetching_history[0] = block_addr;
}

// generate prefetch request
void StreamPrefetcher_V2::do_prefetch()
{
    assert((cfg.PREFETCH_DEGREE == 1) && "Only support PREFETCH_DEGREE 1");

    if (!outq.empty()) {
        uint64_t addr = outq.front() * CacheConfig::CACHELINE_SIZE;
        if (cache->addPrefetch(addr)) {
            update_fetching_history(outq.front());
        }
        outq.pop();   // pop no matter if addPrefetch success or not
    }
}

void StreamPrefetcher_V2::step()
{
    do_prefetch();
}

bool StreamPrefetcher_V2::addTrans(const MappedTransaction &t)
{
    update_ghb(t.addr / CacheConfig::CACHELINE_SIZE);   // cacheline addr
    update_outq();   // Baiyang only update outq when addTrans
    return true;
}
