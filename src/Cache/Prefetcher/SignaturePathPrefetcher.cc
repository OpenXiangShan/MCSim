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

#include "Cache/Prefetcher/SignaturePathPrefetcher.h"
#include "Cache/Cache.h"
#include <cassert>
#include <algorithm>

SignaturePathPrefetcher::SignaturePathPrefetcher()
    : state(State::IDLE),
      cTotal(0),
      cUseful(0),
      stPPN(0),
      curOffset(0),
      curSig(0),
      curCfd(0),
      lookaheadCount(0)
{
    // Initialize Signature Table: stSets x stWays
    signatureTable.resize(stSets);
    for (auto& set : signatureTable) {
        set.resize(stWays);
    }
    stLRU.resize(stSets, 0);

    // Initialize Pattern Table: 512 entries (2^9)
    patternTable.resize(1 << signatureBits);

    // Initialize Prefetch Filter Table: 1024 entries
    prefetchFilter.resize(pftEntries);
}

void SignaturePathPrefetcher::updateSTLRU(size_t set, size_t accessedWay)
{
    stLRU[set] = static_cast<uint8_t>(accessedWay);
}

size_t SignaturePathPrefetcher::getSTVictimWay(size_t set) const
{
    for (size_t w = 0; w < stWays; w++) {
        if (!signatureTable[set][w].valid) {
            return w;
        }
    }
    return (stLRU[set] + 1) % stWays;
}

uint8_t SignaturePathPrefetcher::calculateAlpha() const
{
    if (cTotal == 0) {
        return (unitCfd * 9) / 10;  // 0.9 when no history
    } else if (cUseful >= cTotal) {
        return unitCfd;  // Cap at 1.0
    } else {
        uint8_t alpha = static_cast<uint8_t>((static_cast<uint32_t>(cUseful) << (cfdBits - 1)) / cTotal);
        return (alpha < minAlpha) ? minAlpha : alpha;
    }
}

void SignaturePathPrefetcher::doPrefetch()
{
    if (!prefetchQueue.empty()) {
        uint64_t addr = prefetchQueue.front();
        if (cache->addPrefetch(addr)) {
            prefetchQueue.pop();
        }
    }
}

void SignaturePathPrefetcher::runStateMachine()
{
    switch (state) {
    // ========== s_idle: ST lookup + PT update (combined, single cycle!) ==========
    case State::IDLE: {
        if (!missQueue.empty()) {
            uint64_t lineAddr = missQueue.front();
            missQueue.pop();

            uint64_t ppn = getPPN(lineAddr);
            uint8_t offset = getOffset(lineAddr);
            size_t set = getSTSet(ppn);
            uint64_t tag = getSTTag(ppn);

            stPPN = ppn;
            curOffset = offset;
            curCfd = maxCfd;
            lookaheadCount = 0;

            // ===== ST Lookup (combinational) =====
            bool hit = false;
            size_t hitWay = 0;
            uint8_t lastOffset = 0;
            uint16_t sig = 0;

            for (size_t w = 0; w < stWays; w++) {
                if (signatureTable[set][w].valid && signatureTable[set][w].tag == tag) {
                    hit = true;
                    hitWay = w;
                    lastOffset = signatureTable[set][w].lastOffset;
                    sig = signatureTable[set][w].signature;
                    break;
                }
            }

            int8_t delta = static_cast<int8_t>(offset) - static_cast<int8_t>(lastOffset);
            uint16_t newSig = updateSignature(sig, delta);
            curSig = newSig;

            if (delta != 0) {
                // Update ST
                size_t updateWay = hit ? hitWay : getSTVictimWay(set);
                signatureTable[set][updateWay].valid = true;
                signatureTable[set][updateWay].tag = tag;
                signatureTable[set][updateWay].lastOffset = offset;
                signatureTable[set][updateWay].signature = newSig;
                if (hit) {
                    updateSTLRU(set, hitWay);
                }

                // ===== PT Update (combinational) =====
                PTEntry& entry = patternTable[sig];

                int foundIdx = -1;
                int minIdx = 0;
                uint8_t minCounter = maxCounter;

                for (int i = 0; i < nDeltas; i++) {
                    if (entry.delta[i] == delta) {
                        foundIdx = i;
                        break;
                    }
                    if (entry.cDelta[i] < minCounter) {
                        minCounter = entry.cDelta[i];
                        minIdx = i;
                    }
                }

                int updateIdx = (foundIdx >= 0) ? foundIdx : minIdx;
                uint8_t oldCDelta = (foundIdx >= 0) ? entry.cDelta[updateIdx] : 0;

                if (oldCDelta == maxCounter || entry.cSig == maxCounter) {
                    for (int i = 0; i < nDeltas; i++) {
                        entry.cDelta[i] >>= 1;
                    }
                    entry.cDelta[updateIdx] = (oldCDelta + 1) >> 1;
                    entry.cSig = (entry.cSig + 1) >> 1;
                } else {
                    entry.cDelta[updateIdx] = oldCDelta + 1;
                    entry.cSig = entry.cSig + 1;
                }
                entry.delta[updateIdx] = delta;

                state = State::PROCESS;
            }
            // If delta == 0, stay in IDLE
        }
        break;
    }

    // ========== s_process: First prefetch + start lookahead ==========
    case State::PROCESS: {
        const PTEntry& entry = patternTable[curSig];

        // Find delta with max counter
        int maxIdx = 0;
        uint8_t maxCDelta = 0;
        for (int i = 0; i < nDeltas; i++) {
            if (entry.cDelta[i] > maxCDelta) {
                maxCDelta = entry.cDelta[i];
                maxIdx = i;
            }
        }

        int8_t delta = entry.delta[maxIdx];

        // Calculate confidence
        uint8_t alpha = calculateAlpha();
        uint8_t rDelta = (entry.cSig > 0) ? 
            static_cast<uint8_t>((static_cast<uint32_t>(maxCDelta) << (cfdBits - 1)) / entry.cSig) : 0;
        uint8_t baseCfd = static_cast<uint8_t>((static_cast<uint32_t>(curCfd) * alpha) >> (cfdBits - 1));
        uint8_t cfd = static_cast<uint8_t>((static_cast<uint32_t>(baseCfd) * rDelta) >> (cfdBits - 1));
        curCfd = cfd;

        // Update signature and offset
        uint16_t newSig = updateSignature(curSig, delta);
        int16_t newOffset = static_cast<int16_t>(curOffset) + delta;
        curSig = newSig;
        curOffset = static_cast<uint8_t>(newOffset & ((1 << pageOffsetBits) - 1));

        bool crossPage = (newOffset < 0) || (newOffset >= (1 << pageOffsetBits));
        bool shouldLookahead = (cfd >= prefThreshold) && !crossPage && 
                               (lookaheadCount < maxLookahead);

        if (shouldLookahead) {
            // Calculate prefetch address
            uint64_t prefetchLineAddr = (stPPN << pageOffsetBits) | 
                                        (newOffset & ((1 << pageOffsetBits) - 1));
            uint64_t prefetchAddr = prefetchLineAddr * CacheConfig::CACHELINE_SIZE;

            // Check PFT filter
            size_t pftIdx = getPFTIndex(prefetchLineAddr);
            uint8_t pftTag = getPFTTag(prefetchLineAddr);
            bool pftHit = prefetchFilter[pftIdx].valid && 
                         (prefetchFilter[pftIdx].tag == pftTag);

            if (!pftHit && prefetchQueue.size() < PREFETCH_QUEUE_DEPTH) {
                prefetchQueue.push(prefetchAddr);
                prefetchFilter[pftIdx].valid = true;
                prefetchFilter[pftIdx].tag = pftTag;
                prefetchFilter[pftIdx].useful = false;
                cTotal++;

                if (cTotal >= (1u << 31)) {
                    cTotal >>= 1;
                    cUseful >>= 1;
                }
            }

            lookaheadCount++;
            state = State::LOOKAHEAD;
        } else {
            state = State::IDLE;
        }
        break;
    }

    // ========== s_lookahead: Continue lookahead loop ==========
    case State::LOOKAHEAD: {
        const PTEntry& entry = patternTable[curSig];

        // Find delta with max counter
        int maxIdx = 0;
        uint8_t maxCDelta = 0;
        for (int i = 0; i < nDeltas; i++) {
            if (entry.cDelta[i] > maxCDelta) {
                maxCDelta = entry.cDelta[i];
                maxIdx = i;
            }
        }

        int8_t delta = entry.delta[maxIdx];

        // Calculate confidence (no alpha decay in lookahead, just rDelta)
        uint8_t rDelta = (entry.cSig > 0) ? 
            static_cast<uint8_t>((static_cast<uint32_t>(maxCDelta) << (cfdBits - 1)) / entry.cSig) : 0;
        uint8_t cfd = static_cast<uint8_t>((static_cast<uint32_t>(curCfd) * rDelta) >> (cfdBits - 1));
        curCfd = cfd;

        // Update signature and offset
        uint16_t newSig = updateSignature(curSig, delta);
        int16_t newOffset = static_cast<int16_t>(curOffset) + delta;
        curSig = newSig;
        curOffset = static_cast<uint8_t>(newOffset & ((1 << pageOffsetBits) - 1));

        bool crossPage = (newOffset < 0) || (newOffset >= (1 << pageOffsetBits));
        bool shouldContinue = (cfd >= prefThreshold) && !crossPage && 
                              (lookaheadCount < maxLookahead);

        // Calculate prefetch address
        uint64_t prefetchLineAddr = (stPPN << pageOffsetBits) | 
                                    (newOffset & ((1 << pageOffsetBits) - 1));
        uint64_t prefetchAddr = prefetchLineAddr * CacheConfig::CACHELINE_SIZE;

        // Check PFT filter
        size_t pftIdx = getPFTIndex(prefetchLineAddr);
        uint8_t pftTag = getPFTTag(prefetchLineAddr);
        bool pftHit = prefetchFilter[pftIdx].valid && 
                     (prefetchFilter[pftIdx].tag == pftTag);

        // Issue prefetch if confidence high enough and not filtered
        bool shouldPrefetch = (cfd >= prefThreshold) && !crossPage && !pftHit;

        if (shouldPrefetch && prefetchQueue.size() < PREFETCH_QUEUE_DEPTH) {
            prefetchQueue.push(prefetchAddr);
            prefetchFilter[pftIdx].valid = true;
            prefetchFilter[pftIdx].tag = pftTag;
            prefetchFilter[pftIdx].useful = false;
            cTotal++;

            if (cTotal >= (1u << 31)) {
                cTotal >>= 1;
                cUseful >>= 1;
            }
        }

        if (shouldContinue) {
            lookaheadCount++;
            state = State::LOOKAHEAD;
        } else {
            state = State::IDLE;
        }
        break;
    }

    default:
        state = State::IDLE;
        break;
    }
}

void SignaturePathPrefetcher::step()
{
    doPrefetch();
    runStateMachine();
}

bool SignaturePathPrefetcher::addTrans(const MappedTransaction &t)
{
    uint64_t lineAddr = t.addr / CacheConfig::CACHELINE_SIZE;

    // Check PFT for useful prefetch tracking
    size_t pftIdx = getPFTIndex(lineAddr);
    uint8_t pftTag = getPFTTag(lineAddr);
    PFTEntry& pftEntry = prefetchFilter[pftIdx];

    if (pftEntry.valid && pftEntry.tag == pftTag && !pftEntry.useful) {
        pftEntry.useful = true;
        cUseful++;
    }

    // Add to miss queue
    if (missQueue.size() < MISS_QUEUE_DEPTH) {
        missQueue.push(lineAddr);
        return true;
    }

    return false;
}

void SignaturePathPrefetcher::notifyHit(uint64_t addr)
{
    uint64_t lineAddr = addr / CacheConfig::CACHELINE_SIZE;
    size_t pftIdx = getPFTIndex(lineAddr);
    uint8_t pftTag = getPFTTag(lineAddr);
    PFTEntry& pftEntry = prefetchFilter[pftIdx];

    if (pftEntry.valid && pftEntry.tag == pftTag && !pftEntry.useful) {
        pftEntry.useful = true;
        cUseful++;
    }
}
