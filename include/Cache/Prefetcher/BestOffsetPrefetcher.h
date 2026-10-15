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

#ifndef BEST_OFFSET_PREFETCHER_H
#define BEST_OFFSET_PREFETCHER_H

#include <vector>
#include <cstdint>
#include "config.h"
#include "top/Transaction.h"
#include "Cache/Prefetcher.h"

/**
 * BestOffsetPrefetcher:
 * - Single-way RR table storing tags (not full addresses)
 * - Cross-page detection for prefetches
 */
class BestOffsetPrefetcher : public Prefetcher {
private:
    // ============ Configuration ============
    static constexpr int maxOffset = cfg.BOP_MAX_OFFSET;           // 16
    static constexpr int maxScore = cfg.BOP_MAX_SCORE;             // 15
    static constexpr int maxRounds = cfg.BOP_MAX_ROUNDS;           // 50
    static constexpr int badScore = cfg.BOP_BAD_SCORE;             // 1
    static constexpr int rrIndexBits = cfg.BOP_RR_INDEX_BITS;      // 8
    static constexpr int pageOffsetBits = cfg.BOP_PAGE_OFFSET_BITS; // 7
    static constexpr int rrEntries = 1 << rrIndexBits;             // 256

    // Address width in cache line units
    static constexpr int addrWidth = 21;  // Matches Chisel AddrWidth
    static constexpr int rrTagBits = addrWidth - rrIndexBits;      // 13

    // ============ State Machine ============
    enum class State { LEARN, RESET };
    State state;

    // ============ RR Table (Recent Requests) ============
    // Single-way, stores tags (matching Chisel SyncReadMem)
    std::vector<uint64_t> rr;  // rrEntries elements, each stores rrTagBits

    // ============ Offset List ============
    // Generated using prime filter algorithm
    std::vector<int> offsetList;
    std::vector<uint8_t> scores;  // Score for each offset

    // ============ Learning State ============
    size_t Di;              // Current best offset index
    size_t di;              // Current testing offset index
    uint8_t maxScoreValue;  // Highest score in current phase
    size_t maxScoreDi;      // Index of highest scoring offset
    size_t scoreResetDi;    // Index for resetting scores (in RESET state)
    unsigned int nRounds;   // Current round number
    bool enable;            // Whether to issue prefetches

    // ============ SyncReadMem Timing Simulation ============
    // Chisel uses SyncReadMem which has 1-cycle read latency.
    // We simulate this by storing the pending RR check from previous cycle.
    struct PendingRRCheck {
        bool valid = false;
        uint64_t rrIndex;     // Index to check in RR table
        uint64_t expectedTag; // Tag to compare against
        size_t scoreDi;       // Offset index to update score (RegNext(di))
    } pendingCheck;

    // ============ Prefetch Queue ============
    std::vector<uint64_t> prefetchQueue;
    static constexpr size_t MAX_PREFETCH_QUEUE = 8;

    // ============ Helper Functions ============
    /**
     * Generate offset list using prime filter
     * Excludes multiples of large primes (>=6)
     */
    static std::vector<int> generateOffsetList();

    /**
     * Extract page number from cache line address
     */
    uint64_t getPage(uint64_t lineAddr) const {
        return lineAddr >> pageOffsetBits;
    }

    /**
     * Extract RR index from cache line address
     */
    uint64_t getRRIndex(uint64_t lineAddr) const {
        return lineAddr & ((1ULL << rrIndexBits) - 1);
    }

    /**
     * Extract RR tag from cache line address
     */
    uint64_t getRRTag(uint64_t lineAddr) const {
        return lineAddr >> rrIndexBits;
    }

    /**
     * Transition to RESET state
     */
    void gotoReset();

    /**
     * Process prefetch requests
     */
    void doPrefetch();

public:
    BestOffsetPrefetcher();

    /**
     * Called every cycle by Cache
     */
    void step() override;

    /**
     * Called when Cache receives a memory access
     */
    bool addTrans(const MappedTransaction &t) override;
};

#endif // BEST_OFFSET_PREFETCHER_H  
