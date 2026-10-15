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

#ifndef NEXT_N_LINE_PREFETCHER_H
#define NEXT_N_LINE_PREFETCHER_H

#include <queue>
#include <cstdint>
#include "config.h"
#include "top/Transaction.h"
#include "Cache/Prefetcher.h"

/**
 * NextNLinePrefetcher
 * 
 * A simple prefetcher that prefetches the next N cache lines.
 * Uses a bit vector to control which lines to prefetch.
 * 
 * For example:
 *   - BitVector = 0x1: prefetch next 1 line (offset +1)
 *   - BitVector = 0x3: prefetch next 2 lines (offset +1, +2)
 *   - BitVector = 0xF: prefetch next 4 lines (offset +1, +2, +3, +4)
 * 
 * The prefetcher respects page boundaries and will not prefetch across pages.
 */
class NextNLinePrefetcher : public Prefetcher {
private:
    // Configuration
    static constexpr int QueueDepth = 4;        // address queue depth
    static constexpr int PageOffsetBits = 7;    // 128 cache lines per 8KB page (2^7 = 128)
    
    // Address queue
    std::queue<uint64_t> AddrQueue;
    
    // State machine
    bool idle;                      // True when not processing any miss
    uint64_t currentAddr;           // Current address being processed (block address)
    uint64_t currentBitVector;      // Remaining bits to prefetch
    
    // Default bit vector from config
    uint64_t defaultBitVector;
    
    /**
     * Extract page number from block address
     * Used for cross-page detection
     */
    uint64_t getPage(uint64_t blockAddr) const {
        return blockAddr >> PageOffsetBits;
    }
    
    /**
     * Get the position (1-indexed) of the lowest set bit in the bit vector
     * Returns 0 if no bits are set
     */
    int getLowestSetBitPosition(uint64_t bv) const {
        if (bv == 0) return 0;
        int pos = 1;
        while ((bv & 1) == 0) {
            bv >>= 1;
            pos++;
        }
        return pos;
    }
    
    /**
     * Get the mask for the lowest set bit
     */
    uint64_t getLowestSetBitMask(uint64_t bv) const {
        return bv & (~bv + 1);  // Isolate lowest set bit
    }
    
    /**
     * Process prefetch requests
     */
    void doPrefetch();

public:
    NextNLinePrefetcher();
    
    /**
     * Called every cycle by Cache
     */
    void step() override;
    
    /**
     * Called when Cache receives a memory access
     */
    bool addTrans(const MappedTransaction &t) override;
};

#endif // NEXT_N_LINE_PREFETCHER_H
