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

#include "Cache/SpatialPrefetcher.h"
#include "Cache/Cache.h"

uint64_t SpatialPrefetcher::get_base_block_addr(uint64_t block_addr)
{
    return (block_addr / cfg.PREFETCH_GRANULARITY) * cfg.PREFETCH_GRANULARITY;
}

std::vector<uint64_t> SpatialPrefetcher::get_adj_cachelines(uint64_t block_addr)
{
    std::vector<uint64_t> result;
    uint64_t base_block_addr = get_base_block_addr(block_addr);
    for (int i = 0; i < cfg.PREFETCH_GRANULARITY; i++) {
        result.push_back(base_block_addr + i);
    }
    return result;
}

void SpatialPrefetcher::do_prefetch()
{
    // one cycle add several cachelines into output_queue;
    // Baiyang rtl can set a bundle to do this
    if (!input_queue.empty()) {
        for (auto block_addr : get_adj_cachelines(input_queue.front())) {
            // do not prefetch the requested one
            if (block_addr == input_queue.front()) {
                continue;
            }

            if (output_contains(block_addr)) {
                continue;
            }
            // abort the oldest one
            if (output_queue.full()) {
                output_queue.pop();
            }
            output_queue.push(block_addr);
        }
        input_queue.pop();
    }
}

bool SpatialPrefetcher::input_contains(uint64_t block_addr)
{
    auto in_the_same_gran = [this](uint64_t a, uint64_t b) {
        return get_base_block_addr(a) == get_base_block_addr(b);
    };
    return input_queue.contains(in_the_same_gran, block_addr);
}

bool SpatialPrefetcher::output_contains(uint64_t block_addr)
{
    return output_queue.contains(std::equal_to<uint64_t>(), block_addr);
}

void SpatialPrefetcher::step()
{
    do_prefetch();

    if (!output_queue.empty()) {
        uint64_t addr = output_queue.front() * CacheConfig::CACHELINE_SIZE;
        if (cache->addPrefetch(addr)) {
            output_queue.pop();
        }
    }

}

bool SpatialPrefetcher::addTrans(const MappedTransaction &t)
{
    if (input_contains(t.addr / CacheConfig::CACHELINE_SIZE)) {
        return true;
    }

    // abort the oldest one
    if (input_queue.full()) {
        input_queue.pop();
    }

    input_queue.push(t.addr / CacheConfig::CACHELINE_SIZE);   // cacheline addr
    return true;

}