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

#include "Cache/MainPipe.h"
#include "Cache/Cache.h"
#include "Cache/Prefetcher.h"
#include "AXI2UI/AXI2UI.h"
#include <assert.h>

MainPipe::MainPipe()
    : S1(), S2(), S3()
{
    writeBus.valid = false;
    writeBus.cacheline = new CacheLine();
}

void MainPipe::advance_pipeline()
{
    // ATTENTION: If writeBus isn't written back, don't advance S2 to S3, so that writeBus.req consists with S3.req.
    // ATTENTION: S3.finish here is not the same as Baiyang chisel code by some reasons.
    bool S2_to_S3_advance = ((S3.can_receive() && !writeBus.valid ) && S2.can_advance()) && S2.valid;
    bool S1_to_S2_advance = (S2.can_receive() && S1.can_advance()) && S1.valid;

    // S2 -> S3, normal advance
    if (S2_to_S3_advance) {
        // if this cmd added to urq, flush it in S3
        if (S2_info.urq_done) {
            S3.valid = false;
        } else {
            S3 = S2;
            S3.finish = false;
            S3_info.init();
        }
        S2.valid = false;
        S2_info.init();
    }

    // S1 -> S2, block_req advance
    if (S1.block) {
        assert(S2.valid && !S2.finish);
        assert((S1.block_req.addr == S2.req.addr) && (S1.block_req.cycle == S2.req.cycle));
        // Because we use cacheline pointer, no need to pass value here
    // S1 -> S2, normal advance
    } else if (S1_to_S2_advance && (S2_to_S3_advance || !S2.valid)) {
        S2 = S1;
        S2.finish = false;
        S2_info.init();
        S1.valid = false;
        S1_info.init();
    }
}

void MainPipe::S1_step()
{
    // when block, S1 should handle block_req, so don't return here
    if (!S1.valid || S1.block) {
        return;
    }

    // Only S1 read CacheArray, so no readBus conflict.
    // However, read can be blocked by write.
    // Don't finish when mainPipe.writeBus or refillPipe.writeBus busy
    if (writeBus.valid || cache->has_refill_req) {
        S1.finish = false;
        return;
    }
    
    auto [tag_nc, index, bank] = cache->get_tag_index_bank(S1.req);   // not care tag here
    for (int i = 0; i < cache->ways; i++) {
        S1.cachelines[i] = &(cache->CacheArray[bank][index][i]);
    }
    S1.finish = !S1.block;
}

void MainPipe::S2_step()
{
    // we should update cacheline EVEN FINISHED (but not advanced yet), in case of S3 getting out-of-date info
    if (!S2.valid) {
        return;
    }

    // choose way as the following order:
    // 1. hit way;
    // 2. invalid way;
    // 3. ways with "is_hited" bit true; (this way is hited before)
    // 4. ready way;
    // 5. evict way;
    auto [tag, index_nc, bank_nc] = cache->get_tag_index_bank(S2.req);   // not care index and bank here
    assert(S2.cachelines[0] == &(cache->CacheArray[bank_nc][index_nc][0]));

    // If writeBus contains cacheline of S3 and not written to CacheArray,
    // we forward it to S2, so S2 can get the latest cacheline info
    bool isForwardData = (writeBus.valid && (writeBus.bank == bank_nc) && (writeBus.index == index_nc));
    CacheLine *cachelines[CacheConfig::CACHE_WAYS];
    for (int i = 0; i < cache->ways; i++) {
        cachelines[i] = S2.cachelines[i];
    }

    if (isForwardData) {
        cachelines[writeBus.way] = writeBus.cacheline;
    }

    size_t &way = S2.way_state.way;
    bool &is_hit = S2.way_state.is_hit;
    bool &is_block = S2.way_state.is_block;
    bool &is_all_block = S2.way_state.is_all_block;
    bool &is_invalid = S2.way_state.is_invalid;
    bool &is_ready = S2.way_state.is_ready;
    bool &is_evict = S2.way_state.is_evict;

    is_hit = false;   // match
    is_block = false;   // match but unready
    is_all_block = true;   // all ways is valid and unready
    is_invalid = false;
    is_ready = false;
    is_evict = false;

    // 1. hit way
    for (int i = 0; i < cache->ways; i++) {
        if ((cachelines[i]->tag == tag) && cachelines[i]->valid){
            // assert only one way match
            assert(is_hit == false);
            is_hit = true;
            if (cachelines[i]->ready) {
                is_all_block = false;
            } else {
                is_block = true;
            }
            way = i;
        } else if ((cachelines[i]->valid == false) || (cachelines[i]->ready == true)) {
            is_all_block = false;
        }
    }

    // treat prefetch hit unready as normal hit (because it does nothing)
    if (S2.req.is_prefetch && is_hit && is_block) {
        S2.finish = true;
        return;
    }

    // if block, send block_req to S1
    S1.block = is_block || is_all_block;
    if (S1.block) {
        S1.block_req = S2.req;

        // only send non-prefetch-read cmd to unready queue
        if ((!S2.req.is_prefetch && is_block && S2.req.is_read) && !S2_info.urq_done) {
            if (cache->urq.addTrans(S2.req, way)) {
                S1.block = false;   // added to URQ, so no need to block
                S2_info.urq_done = true;
                S2.finish = true;
                return;
            }
        }

        if (is_block) {
            cache->stats.cacheline_not_ready();
        }
        if (is_all_block) {
            cache->stats.all_cacheline_not_ready();
        }
        S2.finish = false;
        return;
    }

    if (is_hit) {
        // if first hit, send it to prefetcher
        if (cachelines[way]->from_prefetch && !cachelines[way]->is_hited) {
            assert(CacheConfig::pfc_cfg.ENABLE);
            if (CacheConfig::pfc_cfg.FST_HIT_INTO_GHB && !S2.req.is_prefetch && !S2_info.prefetch_done && S2.req.is_read) {
                cache->prefetcher->addTrans(S2.req);
                S2_info.prefetch_done = true;
            }
        }
        // If not block, we should have return, so here is_block always false
        S2.finish = true;
        return;   // not return in for-loop just to check multiple hit
    }

    // if miss, send it to prefetcher
    if (!is_hit) {
        if (CacheConfig::pfc_cfg.ENABLE && !S2.req.is_prefetch && !S2_info.prefetch_done && S2.req.is_read) {
            cache->prefetcher->addTrans(S2.req);
            S2_info.prefetch_done = true;
        }
    }

    // 2. invalid way
    for (int i = cache->ways - 1; i >= 0; i--) {
        if (cachelines[i]->valid == false) {
            is_invalid = true;
            way = i;
            S2.finish = true;
            return;
        }
    }

    // 3. ways with "is_hited" bit true
    uint64_t min = UINT64_MAX;
    for (int i = cache->ways - 1; i >= 0; i--) {
        if (cachelines[i]->is_hited) {
            if (!CacheConfig::LRU_EVICT) {
                way = i;
                S2.finish = true;
                return;
            }
            if (cachelines[i]->last_access_cycle < min) {
                min = cachelines[i]->last_access_cycle;
                way = i;
            }
        }
    }
    if (min != UINT64_MAX) {
        S2.finish = true;
        return;
    }

    // 4. ready way
    min = UINT64_MAX;
    for (int i = cache->ways - 1; i >= 0; i--) {
        if (cachelines[i]->ready) {
            if (!CacheConfig::LRU_EVICT) {
                is_ready = true;
                way = i;
                S2.finish = true;
                return;
            }
            if (cachelines[i]->last_access_cycle < min) {
                min = cachelines[i]->last_access_cycle;
                is_ready = true;
                way = i;
            }
        }
    }
    if (min != UINT64_MAX) {
        S2.finish = true;
        return;
    }

    // 5. evict way
    min = UINT64_MAX;
    is_evict = true;
    if (!CacheConfig::LRU_EVICT) {
        is_ready = true;
        way = rand() % cache->ways;
        S2.finish = true;
        return;
    }
    for (int i = cache->ways - 1; i >= 0; i--) {
        if (cachelines[i]->last_access_cycle < min) {
            min = cachelines[i]->last_access_cycle;
            way = i;
        }
    }
    S2.finish = true;
    return;
}

// This function can't be defined at class CacheArrayWriteBus
// because of lack of definition of class CacheLine in header file.
// So we define it here.
static inline void assign_writeBus(MainPipe::CacheArrayWriteBus *wb, uint64_t index, \
    uint64_t bank, uint64_t way, CacheLine cacheline)
{
    wb->assign(index, bank, way);
    *(wb->cacheline) = cacheline;
}

void MainPipe::S3_step()
{
    // no need to always update state, just update when unfinished
    if (!S3.valid || S3.finish) {
        return;
    }

    auto [tag, index, bank] = cache->get_tag_index_bank(S3.req);
    assert(S3.cachelines[0] == &(cache->CacheArray[bank][index][0]));
    
    CacheLine cacheline = *(S3.cachelines[S3.way_state.way]);   // copy

    // If prefetch hit unready, we treat it as normal hit read,
    // so cacheline.ready may be false in this case.
    if (!(S3.way_state.is_hit && S3.req.is_read && S3.req.is_prefetch)) {
        assert(cacheline.ready || !cacheline.valid);
    }

    // 1. Normal Hit Write
    if (S3.way_state.is_hit && !S3.req.is_read) {
        assert(cacheline.valid && (cacheline.tag == tag));

        if (writeBus.valid) {
            return;   // writeBus busy, fail
        }

        cacheline.is_hited = true;
        cacheline.dirty = true;
        assign_writeBus(&writeBus, index, bank, S3.way_state.way, cacheline);

        cache->stats.write_hit(S3.req, &(cache->CacheArray[bank][index][S3.way_state.way]), cache->get_cycles());
        S3.finish = true;
        return;
    }

    // 2. Normal Hit Read
    if (S3.way_state.is_hit && S3.req.is_read) {
        assert(cacheline.valid && (cacheline.tag == tag));
        if (!S3.req.is_prefetch) {
            if (!cache->returnDataFromMP(S3.req, CallerType::CACHE_HIT)) {
                // TODO: statistics this case
                S3.finish = false;
                return;   // back2am Bus busy, fail
            }
        }
        cache->stats.read_hit(S3.req, &(cache->CacheArray[bank][index][S3.way_state.way]), cache->get_cycles());
        S3.finish = true;
        return;
    }

    auto equal = [](MappedTransaction item, uint64_t addr) { return item.addr == addr; };

    // 3. Full Write Miss
    // For normal write miss, we handle it differently depending on
    // whether prefetch is enabled:
    // (1) If prefetch IS enabled, we add the request to the WCB, but
    //     don't allocate cacheline.
    // (2) If prefetch IS NOT enabled, we don't add the request to the
    //     WCB, but allocate cacheline. (However, need to add evicted
    //     cacheline to WCB if it's dirty.)
    if (!S3.way_state.is_hit && !S3.req.is_read) {
        if (CacheConfig::pfc_cfg.ENABLE) {
            if (cache->wcb->contains(equal, S3.req.addr)) {
                // Overwrite when WCB contains entry with the same addr.
                // But we don't simulate data, so nothing can be done here.
                // TODO: Should this case be counted as a write hit?
                cache->stats.write_hit(S3.req, nullptr, cache->get_cycles());
                S3.finish = true;
                return;
            } else if (cache->wcb->push(S3.req) == false) {
                cache->stats.wcb_full();
                return;   // WCB full, fail
            } else {
                // ATTENTION: After pushing to WCB, we should guarantee S3.finish is true before return,
                //            in case of repeated addition to WCB.
                cache->stats.write_miss(S3.req);
                S3.finish = true;
                return;
            }
        } else {
            if (writeBus.valid) {
                return;   // writeBus busy, fail
            }
            
            // if evict and evicted cacheline undirty, add evicted cacheline to WCB
            if (!S3.way_state.is_invalid && cacheline.dirty) {
                // we should change the initial_transaction of evicted cacheline
                MappedTransaction wcb_trans = cacheline.initial_transaction;
                wcb_trans.is_read = false;
                wcb_trans.is_to_cache = true;   // ATTENTION: don't care!
                wcb_trans.token = 0;   // ATTENTION: don't care!
                if (cache->wcb->push(wcb_trans) == false) {
                    cache->stats.wcb_full();
                    return;   // WCB full, fail
                }
                // ATTENTION: After pushing to WCB, we should guarantee S3.finish is true before return,
                //            in case of repeated addition to WCB.
            }

            // write CacheArray
            cacheline.is_hited = false;
            cacheline.dirty = true;
            cacheline.tag = tag;
            cacheline.valid = true;
            cacheline.from_prefetch = false;
            cacheline.initial_transaction = S3.req;
            cacheline.prefetch_accessed = false;
            cacheline.last_access_cycle = 0;
            cacheline.ready = true;   // we just fill the cacheline with wdata, no need to read from ddr
            assign_writeBus(&writeBus, index, bank, S3.way_state.way, cacheline);

            cache->stats.write_miss(S3.req);
            S3.finish = true;
            return;
        }
    }

    // 4. Read Miss
    // For normal read miss, we handle it differently depending on
    // whether prefetch is enabled:
    // (1) If prefetch IS enabled, we add the request to the MSHR,
    //     ONLY allocate a cacheline for prefetch request, and return
    //     true.
    // (2) If prefetch IS NOT enabled, we add the request to the MSHR,
    //     allocate a cacheline, and return true.
    if (!S3.way_state.is_hit && S3.req.is_read) {
        // directly return when WCB contains entry with the same addr
        auto equal = [](MappedTransaction item, uint64_t addr) { return item.addr == addr; };
        if (cache->wcb->contains(equal, S3.req.addr)) {
            if (!S3.req.is_prefetch) {
                if (cache->returnDataFromMP(S3.req, CallerType::CACHE_HIT)) {
                    cache->stats.read_hit(S3.req, nullptr, cache->get_cycles());
                    S3.finish = true;
                    return;
                } else {
                    // TODO: statistics this case
                    S3.finish = false;
                    return;   // back2am Bus busy, fail
                }
            }

            if (writeBus.valid) {
                return;   // writeBus busy, fail
            }

            // For prefetched cmd miss but hit WCB, directly write it to cacheline.
            // But if evict and evicted cacheline undirty, add evicted cacheline to WCB first.
            if (!S3.way_state.is_invalid && cacheline.dirty) {
                MappedTransaction wcb_trans = cacheline.initial_transaction;
                wcb_trans.is_read = false;
                wcb_trans.is_to_cache = true;   // ATTENTION: don't care!
                wcb_trans.token = 0;   // ATTENTION: don't care!
                if (cache->wcb->push(wcb_trans) == false) {
                    cache->stats.wcb_full();
                    return;   // WCB full, fail
                }
                // ATTENTION: After pushing to WCB, we should guarantee S3.finish is true before return,
                //            in case of repeated addition to WCB.
            }

            // write CacheArray
            cacheline.is_hited = false;
            cacheline.dirty = false;
            cacheline.tag = tag;
            cacheline.valid = true;
            cacheline.from_prefetch = S3.req.is_prefetch;
            cacheline.initial_transaction = S3.req;
            cacheline.prefetch_accessed = false;
            cacheline.last_access_cycle = 0;
            cacheline.ready = true;   // we just fill the cacheline with wdata in WCB, no need to read from ddr
            assign_writeBus(&writeBus, index, bank, S3.way_state.way, cacheline);

            cache->stats.read_hit(S3.req, nullptr, cache->get_cycles());
            S3.finish = true;
            return;
        }

        // Normal Read Miss
        if (cache->mshr->add_request(S3.req, S3.way_state.way) == false) {
            cache->stats.mshr_full();
            return;   // MSHR full, fail
        }

        // prefetch IS enabled and non-prefetch cmd, directly return after added to MSHR
        if (CacheConfig::pfc_cfg.ENABLE && !S3.req.is_prefetch) {
            cache->stats.read_miss(S3.req);
            S3.finish = true;
            return;
        }

        if (writeBus.valid) {
            cache->mshr->remove_request(S3.req);   // in case of multiple add
            return;   // writeBus busy, fail
        }

        // Before write S3.req to cacheline, if evict and evicted cacheline undirty,
        // add the evicted cacheline to WCB
        if (!S3.way_state.is_invalid && cacheline.dirty) {
            MappedTransaction wcb_trans = cacheline.initial_transaction;
            wcb_trans.is_read = false;
            wcb_trans.is_to_cache = true;   // ATTENTION: don't care!
            wcb_trans.token = 0;   // ATTENTION: don't care!
            if (cache->wcb->push(wcb_trans) == false) {
                cache->mshr->remove_request(S3.req);   // in case of multiple add
                cache->stats.wcb_full();
                return;   // WCB full, fail
            }
        }

        // write CacheArray
        cacheline.is_hited = false;
        cacheline.dirty = false;
        cacheline.tag = tag;
        cacheline.valid = true;
        cacheline.from_prefetch = S3.req.is_prefetch;
        cacheline.initial_transaction = S3.req;
        cacheline.prefetch_accessed = false;
        cacheline.last_access_cycle = 0;
        cacheline.ready = false;   // we just add this cmd into MSHR, so we assume data doesn't return yet
        assign_writeBus(&writeBus, index, bank, S3.way_state.way, cacheline);

        cache->stats.read_miss(S3.req);
        S3.finish = true;
        return;
    }
}

bool MainPipe::addTrans(MappedTransaction req)
{
    if (S1.valid) {
        return false;
    }

    S1.req = req;
    S1.valid = true;
    S1.finish = false;
    return true;
}

void MainPipe::step()
{
    advance_pipeline();

    // S3 step: write CacheArray, WCB, MSHR
    if (S3.valid) {
        S3_step();
    }

    // S2 step: generate way, is_hit, is_block, is_all_block etc, and prefetch
    if (S2.valid) {
        S2_step();
    }

    // S1 step: read CacheArray
    if (S1.valid) {
        S1_step();
    }
}