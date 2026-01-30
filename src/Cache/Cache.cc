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

#include "Cache/Cache.h"
#include "AXI2UI/AXI2UI.h"
#include "Scheduler/TransactionSchedule.h"
#include "AddrMap/AddrMap.h"
#include "Cache/Prefetcher.h"

static inline void dump_prefetch(uint64_t addr)
{
    if (DumpConfig::DUMP_PREFETCH) {
        std::cout << "prefetch 0x" << std::hex << addr << std::dec << std::endl;
    }
}

Cache::Cache()
    : cmdQueue(CacheConfig::CMD_QUEUE_DEPTH),
      prefetchQueue(1),
      cache_size(CacheConfig::CACHE_SIZE),
      cacheline_size(CacheConfig::CACHELINE_SIZE),
      ways(CacheConfig::CACHE_WAYS),
      num_banks(CacheConfig::CACHE_BANKS),
      mshr(new MSHR(CacheConfig::MSHR_DEPTH)),
      wcb(new FixedQueue<MappedTransaction>(CacheConfig::WCB_DEPTH)),
      prefetcher(new Prefetcher),
      // statistics
      stats()
{
    num_sets = cache_size / ways / num_banks / cacheline_size;
    CacheArray.resize(num_banks);
    for (auto &bank : CacheArray) {
        bank.resize(num_sets);
        for (auto &set : bank) {
            set.resize(ways);
        }
    }
    prefetcher->cache = this;
}

void Cache::set_axi2ui(AXI2UI *axi2ui)
{
    this->axi2ui = axi2ui;
}

rw_trans_type Cache::addTrans(const MappedTransaction &r, const MappedTransaction &w, rw_trans_type t)
{
    // from AddrMap
    if (t == RD_ONLY) {
        if (cmdQueue.push(r) == false) {
            return NO_RW;
        }
        return RD_ONLY;
    } else if (t == WR_ONLY) {
        if (cmdQueue.push(w) == false) {
            return NO_RW;
        }
        return WR_ONLY;
    } else if (t == RD_WR) {
        // read first, but if addr is the same, send cmd with earlier cycle
        if (r.addr == w.addr && w.cycle < r.cycle) {
        // send cmd with earlier cycle
        // if (w.cycle < r.cycle) {
            if (cmdQueue.push(w) == false) {
                return NO_RW;
            }
            return WR_ONLY;
        } else {
            if (cmdQueue.push(r) == false) {
                return NO_RW;
            }
            return RD_ONLY;
        }
    } else {
        return NO_RW;
    }
}

std::pair<bool, CacheLineQueryResult> Cache::addCacheLine(const MappedTransaction &t)
{
    auto [tag, index, bank] = get_tag_index_bank(t);
    size_t way;

    bool is_hit = false;   // match and ready
    bool is_block = false;   // match but block
    bool is_all_block = true;   // not match but all ways is valid and block
    for (size_t i = 0; i < ways; i++) {
        if (CacheArray[bank][index][i].tag == tag && CacheArray[bank][index][i].valid){
            // assert only one way match
            assert(is_hit == false && is_block == false);
            if (CacheArray[bank][index][i].ready) {
                is_hit = true;
            } else {
                is_block = true;
            }
            is_all_block = false;
            way = i;
        } else if (CacheArray[bank][index][i].valid == false || CacheArray[bank][index][i].ready == true) {
            is_all_block = false;
        }
    }

    if (t.is_read) {
        if (is_hit) {
            // 1. Normal Read Hit
            if (!t.is_prefetch) {
                axi2ui->put_back(t.id, t.token, CallerType::CACHE_HIT);
                // After cacheline-from-prefetch is hit at the first time,
                // we should add it to prefetcher.
                // The prefetch_accessed will be set to true in stats.read_hit(), so
                // we check it here to know if it is the first hit.
                if (CacheArray[bank][index][way].from_prefetch && !CacheArray[bank][index][way].prefetch_accessed) {
                    assert(CacheConfig::pfc_cfg.ENABLE);
                    if (CacheConfig::pfc_cfg.HIT_INTO_GHB) {
                        prefetcher->addTrans(t);
                    }
                }
            }
            stats.read_hit(t, &(CacheArray[bank][index][way]));
            return std::make_pair(true, CacheLineQueryResult::HIT);
        } else {
            // 2. Read Miss
            // block when is_block or is_all_block
            if (is_block || is_all_block) {
                // TODO: merge it to corresponding mshr entry when is_block (Baiyang doesn't use this method)
                stats.cacheline_not_ready();
                if (is_all_block) {
                    stats.all_cacheline_not_ready();
                }
                return std::make_pair(false, CacheLineQueryResult::BLOCK);
            }
            // directly return when WCB contains entry with the same addr
            auto equal = [](MappedTransaction item, uint64_t addr) { return item.addr == addr; };
            if (wcb->contains(equal, t.addr)) {
                if (!t.is_prefetch) {
                    axi2ui->put_back(t.id, t.token, CallerType::CACHE_HIT);
                }
                stats.read_hit(t, nullptr);
                return std::make_pair(true, CacheLineQueryResult::HIT);
            }
            // For normal read miss, we handle it differently depending on
            // whether prefetch is enabled:
            // (1) If prefetch IS enabled, we add the request to the MSHR,
            //     ONLY allocate a cacheline for prefetch request, and return
            //     true.
            // (2) If prefetch IS NOT enabled, we add the request to the MSHR,
            //     allocate a cacheline, and return true.
            if (mshr->add_request(t) == false) {
                stats.mshr_full();
                return std::make_pair(false, CacheLineQueryResult::MSHR_FULL);
            }
            if (CacheConfig::pfc_cfg.ENABLE) {
                if (t.is_prefetch) {
                    // if add cacheline failed (usually due to wcb full), block
                    if (addMissedCacheLine(bank, index, tag, false, false, t) == false) {
                        mshr->remove_request(t);
                        stats.wcb_full();
                        return std::make_pair(false, CacheLineQueryResult::WCB_FULL);
                    }
                } else {
                    prefetcher->addTrans(t);
                }
            } else {
                // if add cacheline failed (usually due to wcb full), block
                if (addMissedCacheLine(bank, index, tag, false, false, t) == false) {
                    mshr->remove_request(t);
                    stats.wcb_full();
                    return std::make_pair(false, CacheLineQueryResult::WCB_FULL);
                }
            }
            stats.read_miss(t);
            return std::make_pair(true, CacheLineQueryResult::MISS);
        }
    } else {
        if (is_hit) {
            // 3. Normal Write Hit
            assert(CacheArray[bank][index][way].valid);
            if (CacheConfig::pfc_cfg.ENABLE) {
                // After cacheline-from-prefetch is hit at the first time,
                // we should add it to prefetcher.
                // The prefetch_accessed will be set to true in stats.write_hit(), so
                // we check it here to know if it is the first hit.
                if (CacheArray[bank][index][way].from_prefetch && !CacheArray[bank][index][way].prefetch_accessed) {
                    if (CacheConfig::pfc_cfg.HIT_INTO_GHB) {
                        prefetcher->addTrans(t);
                    }
                }
            }
            stats.write_hit(t, &(CacheArray[bank][index][way]));
            CacheArray[bank][index][way].dirty = true;
            return std::make_pair(true, CacheLineQueryResult::HIT);
        } else {
            // 4. Full Write Miss (full cacheline write)
            // block when is_block or is_all_block
            if (is_block || is_all_block) {
                stats.cacheline_not_ready();
                if (is_all_block) {
                    stats.all_cacheline_not_ready();
                }
                return std::make_pair(false, CacheLineQueryResult::BLOCK);
            }
            // update data when WCB contains entry with the same addr
            auto equal = [](MappedTransaction item, uint64_t addr) { return item.addr == addr; };
            if (wcb->contains(equal, t.addr)) {
                // we don't simulate data, so return directly
                stats.write_hit(t, nullptr);
                return std::make_pair(true, CacheLineQueryResult::HIT);
            }
            // For normal write miss, we handle it differently depending on
            // whether prefetch is enabled:
            // (1) If prefetch IS enabled, we add the request to the WCB, but
            //     don't allocate cacheline.
            // (2) If prefetch IS NOT enabled, we don't add the request to the
            //     WCB, but allocate cacheline.
            if (CacheConfig::pfc_cfg.ENABLE) {
                if (wcb->push(t) == false) {
                    stats.wcb_full();
                    return std::make_pair(false, CacheLineQueryResult::WCB_FULL);
                }
                prefetcher->addTrans(t);
            } else {
                if (addMissedCacheLine(bank, index, tag, true, true, t) == false) {
                    stats.wcb_full();
                    return std::make_pair(false, CacheLineQueryResult::WCB_FULL);
                }
            }
            stats.write_miss(t);
            return std::make_pair(true, CacheLineQueryResult::MISS);
            // TODO: 5. Partial Write Miss (partial cacheline write)
        }
    }
}

bool Cache::addMissedCacheLine(size_t bank, size_t index, uint64_t tag, bool dirty, bool ready, MappedTransaction t)
{
    size_t way = allocateCacheLine(bank, index);
    if (way == (size_t)(-1)) {
        return false;
    }
    CacheArray[bank][index][way].valid = true;
    CacheArray[bank][index][way].tag = tag;
    CacheArray[bank][index][way].dirty = dirty;
    CacheArray[bank][index][way].ready = ready;
    CacheArray[bank][index][way].initial_transaction = t;
    CacheArray[bank][index][way].from_prefetch = t.is_prefetch;
    CacheArray[bank][index][way].prefetch_accessed = false;
    return true;
}

size_t Cache::evictCacheLine(size_t bank, size_t index)
{
    size_t way = 0;

    for (size_t way_i = 0; way_i < ways; ++way_i) {
        // if prefetch_accessed entry exists, return it
        if (CacheArray[bank][index][way_i].prefetch_accessed == true) {
            CacheArray[bank][index][way_i].valid = false;
            return way_i;
        }
    }

    std::vector<size_t> ready_ways;
    for (size_t way_i = 0; way_i < ways; ++way_i) {
        // don't include cacheline that is valid and block
        if (CacheArray[bank][index][way_i].valid && CacheArray[bank][index][way_i].ready == false) {
            continue;
        }
        // if undirty entry exists, return it
        if (CacheArray[bank][index][way_i].dirty == false) {
            CacheArray[bank][index][way_i].valid = false;
            return way_i;
        }
        ready_ways.push_back(way_i);
    }
    assert(ready_ways.empty() == false);

    way = ready_ways[rand() % ready_ways.size()];

    assert(CacheArray[bank][index][way].valid && CacheArray[bank][index][way].dirty);
    CacheArray[bank][index][way].initial_transaction.is_read = false;
    CacheArray[bank][index][way].initial_transaction.is_to_cache = true;   // ATTENTION: don't care!!!
    CacheArray[bank][index][way].initial_transaction.token = 0;   // ATTENTION: don't care!!!
    if (wcb->push(CacheArray[bank][index][way].initial_transaction) == false) {
        return (size_t)(-1);
    }
    return way;
}

size_t Cache::allocateCacheLine(size_t bank, size_t index)
{
    size_t way = 0;
    for (size_t i = 0; i < ways; i++) {
        if (CacheArray[bank][index][i].valid == false) {
            return i;
        }
    }
    return evictCacheLine(bank, index);
}

std::tuple<size_t, size_t, size_t> Cache::get_tag_index_bank(MappedTransaction t)
{
    size_t tag, index, bank;
    // | rank(1) | bank(2) | bg(2) | row(16) | col(10) | 3'b0 |
    uint64_t fake_addr = (((uint64_t)t.mapped_addr.rank & 0x1) << 33)
                       | (((uint64_t)t.mapped_addr.bank & 0x3) << 31)
                       | (((uint64_t)t.mapped_addr.bg & 0x3) << 29)
                       | (((uint64_t)t.mapped_addr.row & 0xffff) << 13)
                       | (((uint64_t)t.mapped_addr.col & 0x3ff) << 3);
    // uint64_t fake_addr = t.addr;
    // | tag | index(10) | bank(3) | 6'b0 |
    tag = fake_addr >> 19;
    index = (fake_addr >> 9) & 0x3ff;
    bank = (fake_addr >> 6) & 0x7;
    return std::make_tuple(tag, index, bank);
};

void Cache::returnFromScg(MappedTransaction t)
{
    auto mshr_it = mshr->get_mshr_entry(t);

    // return data to axi2ui
    if (!t.is_prefetch) {
        axi2ui->put_back(t.id, t.token, CallerType::CACHE_MISS);
    }
    
    // We handle data from scg differently depending
    // on whether prefetch is enabled:
    // 1. If prefetch IS enabled, we only add data required
    //    by prefetch to the cache.
    // 2. If prefetch IS NOT enabled, we add all data to the cache.
    if (CacheConfig::pfc_cfg.ENABLE && t.is_prefetch || !CacheConfig::pfc_cfg.ENABLE) {
        // set cacheline ready
        bool is_match = false;
        auto [tag, index, bank] = get_tag_index_bank(t);
        for (size_t i = 0; i < ways; i++) {
            if (CacheArray[bank][index][i].valid && CacheArray[bank][index][i].tag == tag){
                is_match = true;
                CacheArray[bank][index][i].ready = true;
                assert(CacheArray[bank][index][i].from_prefetch == t.is_prefetch);
                assert(CacheArray[bank][index][i].prefetch_accessed == false);
            }
        }
        assert(is_match);
    }

    mshr->finish_request(t);
}

void Cache::step()
{
    // MSHR step
    auto [t, get_success] = mshr->get_unissued_request();
    if (get_success) {
        assert(t.is_read);
        rw_trans_type type = scheduler->addTrans(t, t, RD_ONLY, CallerType::CACHE_MISS);
        assert(type == RD_ONLY || type == NO_RW);
        if (type == RD_ONLY) {
            mshr->issue_request(t);
        }
    }

    // WCB step
    if (!wcb->empty()) {
        MappedTransaction t = wcb->front();
        rw_trans_type type = scheduler->addTrans(t, t, WR_ONLY, CallerType::CACHE_WRITEBACK);
        assert(type == WR_ONLY || type == NO_RW);
        if (type == WR_ONLY) {
            wcb->pop();
        }
    }

    // Prefetcher step
    if (CacheConfig::pfc_cfg.ENABLE) {
    // for (int i = 0; i < 16; i++) {
        prefetcher->step();
    // }
    }

    // stage3 step

    // stage2 step

    // stage1 step: read CacheArray
    // priority: forward(prefetchQueue) > cmdQueue > prefetchQueue
    bool has_cmd = !cmdQueue.empty();
    bool has_prefetch = !prefetchQueue.empty();
    auto it = std::find_if(prefetchQueue.begin(), prefetchQueue.end(),
        [&](const auto prefetch) {
            return cmdQueue.contains(
                [](const auto cmd, const auto prefetch) {
                    return cmd.addr == prefetch;
                },
                prefetch
            );
        }
    );
    bool has_forward = (it != prefetchQueue.end());
    if (has_forward && CacheConfig::pfc_cfg.ENABLE) {
        MappedTransaction forward_t;
        forward_t.addr = *it;
        forward_t.is_to_cache = true;
        forward_t.is_prefetch = true;
        forward_t.is_read = true;
        forward_t.mapped_addr = linear_to_mapped(*it);
        auto add_result = addCacheLine(forward_t);
        if (add_result.first == true) {
            prefetchQueue.erase(it);
            // prefetch new cacheline, dump it and statistics
            if (add_result.second == CacheLineQueryResult::MISS) {
                dump_prefetch(*it);
                stats.prefetch();
            }
        }
    } else if (has_cmd) {
        MappedTransaction t = cmdQueue.front();
        if (addCacheLine(t).first == true) {
            cmdQueue.pop();
        }
    } else if (has_prefetch && CacheConfig::pfc_cfg.ENABLE) {
        uint64_t addr = prefetchQueue.front();
        MappedTransaction prefetch_t;
        prefetch_t.addr = addr;
        prefetch_t.is_to_cache = true;
        prefetch_t.is_prefetch = true;
        prefetch_t.is_read = true;
        prefetch_t.mapped_addr = linear_to_mapped(addr);
        auto add_result = addCacheLine(prefetch_t);
        if (add_result.first == true) {
            prefetchQueue.pop();
            // prefetch new cacheline, dump it and statistics
            if (add_result.second == CacheLineQueryResult::MISS) {
                dump_prefetch(addr);
                stats.prefetch();
            }
        }
    }
}

bool Cache::addPrefetch(uint64_t addr)
{
    if (prefetchQueue.full()) {
        return false;
    }
    if (!prefetchQueue.contains(std::equal_to<uint64_t>(), addr)) {
        prefetchQueue.push(addr);
    }
    return true;
}

void Cache::statistics() const
{
    stats.print();
}
