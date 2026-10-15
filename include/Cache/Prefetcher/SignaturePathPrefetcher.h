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

#ifndef SIGNATURE_PATH_PREFETCHER_H
#define SIGNATURE_PATH_PREFETCHER_H

#include <vector>
#include <queue>
#include <cstdint>
#include <algorithm>
#include "config.h"
#include "top/Transaction.h"
#include "Cache/Prefetcher.h"

/**
 * SignaturePathPrefetcherOpt:
 * - Register-based ST and PT (single-cycle access)
 * - 3-state FSM: IDLE -> PROCESS -> LOOKAHEAD
 * - Signature Table (ST): 64 sets × 4 ways, LRU replacement
 * - Pattern Table (PT): 512 entries × 4 deltas per entry
 * - Prefetch Filter Table (PFT): 1024 entries
 * - Max lookahead depth: 8
 */
class SignaturePathPrefetcher : public Prefetcher {
private:
    // ============ Configuration ============
    static constexpr int pageOffsetBits = cfg.SPP_PAGE_OFFSET_BITS;  // 7
    static constexpr int stSets = cfg.SPP_ST_SETS;                   // 64
    static constexpr int stWays = cfg.SPP_ST_WAYS;                   // 4
    static constexpr int signatureBits = cfg.SPP_SIGNATURE_BITS;     // 9 (reduced!)
    static constexpr int nDeltas = cfg.SPP_N_DELTAS;                 // 4
    static constexpr int counterBits = cfg.SPP_COUNTER_BITS;         // 4
    static constexpr int cfdBits = cfg.SPP_CFD_BITS;                 // 7
    static constexpr int prefThreshold = cfg.SPP_PREF_THRESHOLD;     // 32
    static constexpr int minAlpha = cfg.SPP_MIN_ALPHA;               // 16
    static constexpr int pftEntries = cfg.SPP_PFT_ENTRIES;           // 1024
    static constexpr int pftTagBits = cfg.SPP_PFT_TAG_BITS;          // 6
    static constexpr int maxLookahead = cfg.SPP_MAX_LOOKAHEAD;       // 8

    static constexpr int maxCounter = (1 << counterBits) - 1;        // 15
    static constexpr int maxCfd = (1 << cfdBits) - 1;                // 127
    static constexpr int unitCfd = 1 << (cfdBits - 1);               // 64

    // ============ Signature Table Entry ============
    struct STEntry {
        uint64_t tag = 0;
        uint8_t lastOffset = 0;
        uint16_t signature = 0;
        bool valid = false;
    };

    // ============ Pattern Table Entry ============
    struct PTEntry {
        int8_t delta[nDeltas] = {0};
        uint8_t cDelta[nDeltas] = {0};
        uint8_t cSig = 0;
    };

    // ============ Prefetch Filter Table Entry ============
    struct PFTEntry {
        bool valid = false;
        uint8_t tag = 0;
        bool useful = false;
    };

    // ============ Tables ============
    std::vector<std::vector<STEntry>> signatureTable;  // [stSets][stWays]
    std::vector<uint8_t> stLRU;                        // LRU state per set
    std::vector<PTEntry> patternTable;                 // [1 << signatureBits]
    std::vector<PFTEntry> prefetchFilter;              // [pftEntries]

    // ============ Accuracy Counters ============
    uint32_t cTotal;   // Total prefetches issued
    uint32_t cUseful;  // Useful prefetches (demand hit on prefetched line)

    // ============ Miss Queue ============
    std::queue<uint64_t> missQueue;
    static constexpr size_t MISS_QUEUE_DEPTH = cfg.SPP_MISS_QUEUE_DEPTH;  // 16

    // ============ Prefetch Queue ============
    std::queue<uint64_t> prefetchQueue;
    static constexpr size_t PREFETCH_QUEUE_DEPTH = cfg.SPP_PREF_QUEUE_DEPTH;  // 32

    // ============ Lookahead State ============
    uint64_t stPPN;        // Page number from ST lookup
    uint8_t curOffset;     // Current offset within page
    uint16_t curSig;       // Current signature for lookahead
    uint8_t curCfd;        // Current confidence
    uint8_t lookaheadCount; // Lookahead depth counter

    // ============ 3-State FSM (matching Chisel Opt version) ============
    enum class State {
        IDLE,       // s_idle: Wait for miss, do ST lookup + PT update
        PROCESS,    // s_process: First prefetch + start lookahead
        LOOKAHEAD   // s_lookahead: Continue lookahead loop
    };
    State state;

    // ============ Helper Functions ============

    uint64_t getPPN(uint64_t lineAddr) const {
        return lineAddr >> pageOffsetBits;
    }

    uint8_t getOffset(uint64_t lineAddr) const {
        return lineAddr & ((1 << pageOffsetBits) - 1);
    }

    size_t getSTSet(uint64_t ppn) const {
        return ppn & (stSets - 1);
    }

    uint64_t getSTTag(uint64_t ppn) const {
        return ppn >> 6;  // lg(stSets) = 6
    }

    uint16_t updateSignature(uint16_t sig, int8_t delta) const {
        return ((sig << 3) ^ static_cast<uint16_t>(delta & 0xFF)) & 
                ((1 << signatureBits) - 1);
    }

    size_t getPFTIndex(uint64_t addr) const {
        return addr & (pftEntries - 1);
    }

    uint8_t getPFTTag(uint64_t addr) const {
        return (addr >> 10) & ((1 << pftTagBits) - 1);
    }

    /**
     * Calculate alpha: α = c_useful / c_total
     */
    uint8_t calculateAlpha() const;

    /**
     * Update LRU for ST access
     */
    void updateSTLRU(size_t set, size_t accessedWay);

    /**
     * Get LRU victim way for ST
     */
    size_t getSTVictimWay(size_t set) const;

    /**
     * Process prefetch queue - send prefetches to cache
     */
    void doPrefetch();

    /**
     * Run 3-state machine step
     */
    void runStateMachine();

public:
    SignaturePathPrefetcher();
    void step() override;
    bool addTrans(const MappedTransaction &t) override;
    void notifyHit(uint64_t addr);
};

#endif // SIGNATURE_PATH_PREFETCHER_H
