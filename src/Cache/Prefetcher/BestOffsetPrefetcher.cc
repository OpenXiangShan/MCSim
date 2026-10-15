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

#include "Cache/Prefetcher/BestOffsetPrefetcher.h"
#include "Cache/Cache.h"
#include <cassert>
#include <iostream>

// Generate offset list using prime filter algorithm
std::vector<int> BestOffsetPrefetcher::generateOffsetList()
{
    // Step 1: Find primes using sieve
    std::vector<bool> isPrime(maxOffset + 1, true);
    for (int i = 2; i <= maxOffset; i++) {
        if (isPrime[i]) {
            for (int j = i + i; j <= maxOffset; j += i) {
                isPrime[j] = false;
            }
        }
    }

    // Step 2: Exclude multiples of large primes (>= 6)
    std::vector<bool> excluded(maxOffset + 1, false);
    for (int i = 6; i <= maxOffset; i++) {
        if (isPrime[i]) {
            for (int j = i; j <= maxOffset; j += i) {
                excluded[j] = true;
            }
        }
    }

    // Step 3: Generate list of non-excluded offsets
    std::vector<int> result;
    for (int i = 1; i <= maxOffset; i++) {
        if (!excluded[i]) {
            result.push_back(i);
        }
    }

    // Result should be: [1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16]
    return result;
}

BestOffsetPrefetcher::BestOffsetPrefetcher()
    :   state(State::RESET),
        Di(0),
        di(0),
        maxScoreValue(0),
        maxScoreDi(0),
        scoreResetDi(0),
        nRounds(0),
        enable(false)
{
    // Initialize RR table (256 entries, all zeros)
    rr.resize(rrEntries, 0);

    // Generate offset list using prime filter
    offsetList = generateOffsetList();

    // Initialize scores (one per offset)
    scores.resize(offsetList.size(), 0);

    // Reserve prefetch queue
    prefetchQueue.reserve(MAX_PREFETCH_QUEUE);
}

void BestOffsetPrefetcher::gotoReset()
{
    state = State::RESET;
    scoreResetDi = 0;
    enable = (maxScoreValue > badScore);
    Di = maxScoreDi;
}

void BestOffsetPrefetcher::doPrefetch()
{
    // Try to send one prefetch per cycle
    if (!prefetchQueue.empty()) {
        uint64_t addr = prefetchQueue.front();
        if (cache->addPrefetch(addr)) {
            // Successfully issued, remove from queue
            prefetchQueue.erase(prefetchQueue.begin());
        }
    }
}

void BestOffsetPrefetcher::step()
{
    // Issue pending prefetches
    doPrefetch();

    // Handle RESET state: reset one score per cycle
    if (state == State::RESET) {
        scores[scoreResetDi] = 0;

        if (scoreResetDi == offsetList.size() - 1) {
            // Finished resetting, go to LEARN
            state = State::LEARN;
            di = 0;
            nRounds = 0;
            maxScoreValue = 0;
        } else {
            scoreResetDi++;
        }
    }
}

bool BestOffsetPrefetcher::addTrans(const MappedTransaction &t)
{
    // Convert byte address to cache line address (64B-aligned)
    uint64_t lineAddr = t.addr / CacheConfig::CACHELINE_SIZE;

    // Current page for cross-page detection
    uint64_t page = getPage(lineAddr);

    // Best offset value
    int D = offsetList[Di];
    // Current test offset value
    int d = offsetList[di];

    // ========== 1. Make Prediction (Issue Prefetch) ==========
    if (enable) {
        uint64_t predLine = lineAddr + D;
        uint64_t predPage = getPage(predLine);

        // Only prefetch if same page (matching Chisel cross-page check)
        if (page == predPage) {
            uint64_t prefetchAddr = predLine * CacheConfig::CACHELINE_SIZE;

            // Add to queue if not full and not duplicate
            if (prefetchQueue.size() < MAX_PREFETCH_QUEUE) {
                bool exists = false;
                for (const auto& addr : prefetchQueue) {
                    if (addr == prefetchAddr) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    prefetchQueue.push_back(prefetchAddr);
                }
            }
        }
    }

    // ========== 2. Process Previous Cycle's RR Check (SyncReadMem 1-cycle delay) ==========
    // Chisel: when (RegNext(rrReadEn) && RegNext(tag) === rrTag) { scores(RegNext(di)) := ... }
    // We simulate this by processing pendingCheck which was set up in the previous call
    if (pendingCheck.valid) {
        // Now check if the tag matches (simulating the delayed read result)
        if (rr[pendingCheck.rrIndex] == pendingCheck.expectedTag) {
            // Score hit! Increment score for the offset from previous cycle
            scores[pendingCheck.scoreDi]++;
            uint8_t score = scores[pendingCheck.scoreDi];

            // Update max score tracking
            // When two offsets have same score, prefer smaller index (matching Chisel)
            if (score > maxScoreValue || (score == maxScoreValue && pendingCheck.scoreDi < maxScoreDi)) {
                maxScoreValue = score;
                maxScoreDi = pendingCheck.scoreDi;
            }

            // Check if reached max score
            if (score == maxScore) {
                gotoReset();
            }
        }
        pendingCheck.valid = false;
    }

    // ========== 3. Setup RR Check for Next Cycle (will be processed next addTrans call) ==========
    if (state == State::LEARN) {
        // Calculate base line: line - d
        uint64_t baseLine = lineAddr - d;
        uint64_t basePage = getPage(baseLine);

        // Only setup pending check if base is in same page
        // Chisel: val rrReadEn = miss_valid && page === basePage && state === s_learn
        if (page == basePage) {
            pendingCheck.valid = true;
            pendingCheck.rrIndex = getRRIndex(baseLine);
            pendingCheck.expectedTag = getRRTag(baseLine);
            pendingCheck.scoreDi = di;  // RegNext(di) - captured for next cycle
        } else {
            pendingCheck.valid = false;
        }

        // ========== 4. Increment Offset Index and Round ==========
        if (di != offsetList.size() - 1) {
            di++;
        } else {
            di = 0;
            if (nRounds != maxRounds - 1) {
                nRounds++;
            } else {
                // Reached max rounds, force reset
                gotoReset();
            }
        }
    } else {
        pendingCheck.valid = false;
    }

    // ========== 5. Update RR Table ==========
    uint64_t baseLine;
    bool rrWriteEn = false;

    if (enable) {
        // When prefetch is on, record Y - D
        baseLine = lineAddr - D;
        uint64_t basePage = getPage(baseLine);
        rrWriteEn = (page == basePage);
    } else {
        // When prefetch is off, record Y itself
        baseLine = lineAddr;
        rrWriteEn = true;
    }

    if (rrWriteEn) {
        uint64_t index = getRRIndex(baseLine);
        uint64_t tag = getRRTag(baseLine);
        rr[index] = tag;
    }

    return true;
}
