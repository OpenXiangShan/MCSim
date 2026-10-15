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

#include "Cache/Prefetcher/NextNLinePrefetcher.h"
#include "Cache/Cache.h"
#include <cassert>
#include <iostream>

NextNLinePrefetcher::NextNLinePrefetcher()
    :   idle(true),
        currentAddr(0),
        currentBitVector(0),
        defaultBitVector(cfg.NEXT_N_LINE_BIT_VECTOR)
{
    // Nothing else to initialize
}

void NextNLinePrefetcher::doPrefetch()
{
    // If idle and queue has entries, start processing
    if (idle && !AddrQueue.empty()) {
        currentAddr = AddrQueue.front();
        currentBitVector = defaultBitVector;
        idle = false;
    }
    
    // If not processing anything, return
    if (idle) {
        return;
    }
    
    // Check if all bits have been processed
    if (currentBitVector == 0) {
        // Done with this address, pop from queue and go idle
        if (!AddrQueue.empty()) {
            AddrQueue.pop();
        }
        idle = true;
        return;
    }
    
    // Get the step (offset) from the lowest set bit position
    int step = getLowestSetBitPosition(currentBitVector);
    if (step == 0) {
        // No bits set, done
        currentBitVector = 0;
        return;
    }
    
    // Calculate prefetch address (block address + offset)
    uint64_t predBlockAddr = currentAddr + step;
    
    // Check if prefetch address is in the same page
    bool samePage = getPage(currentAddr) == getPage(predBlockAddr);
    
    // Clear the current bit we're processing
    uint64_t mask = getLowestSetBitMask(currentBitVector);
    
    if (samePage) {
        // Convert block address to byte address for cache->addPrefetch
        uint64_t prefetchAddr = predBlockAddr * CacheConfig::CACHELINE_SIZE;
        
        // Try to send prefetch request
        if (cache->addPrefetch(prefetchAddr)) {
            // Success, move to next bit
            currentBitVector ^= mask;
        }
        // If failed, retry next cycle (don't clear the bit)
    } else {
        // Cross-page, skip this and remaining prefetches
        currentBitVector = 0;
    }
}

void NextNLinePrefetcher::step()
{
    // Process prefetch requests each cycle
    doPrefetch();
}

bool NextNLinePrefetcher::addTrans(const MappedTransaction &t)
{ 
    // Convert byte address to block address (64B aligned)
    uint64_t blockAddr = t.addr / CacheConfig::CACHELINE_SIZE;
    
    // Check queue is not full (simple overflow protection)
    if (AddrQueue.size() < QueueDepth) {
        AddrQueue.push(blockAddr);
        return true;
    }
    
    return false;
}
