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
#include "top/Controller.h"
#include "Scheduler/TransactionSchedule.h"
#include "AddrMap/AddrMap.h"
#include "utils/log.h"
#include "Cache/StreamPrefetcher_V2.h"
#include "Cache/SpatialPrefetcher.h"
#include "Cache/Prefetcher/BestOffsetPrefetcher.h"   
#include "Cache/Prefetcher/NextNLinePrefetcher.h"
#include "Cache/Prefetcher/SignaturePathPrefetcher.h"
#include "Cache/Prefetcher/SmartScanPrefetcher_v2.h"
#include <queue>

// ATTENTION: tmp_urqback_queue is now a per-Cache instance member
// (see include/Cache/Cache.h). Was file-scope static, shared across
// both channels in dual-channel mode, causing cross-channel corruption.


static inline void dump_prefetch(uint64_t addr)
{
    if (DumpConfig::DUMP_PREFETCH) {
        std::cout << "prefetch 0x" << std::hex << addr << std::dec << std::endl;
    }
}

Cache::Cache()
    : cmdQueue(CacheConfig::CMD_QUEUE_DEPTH),
      prefetchQueue(1),
      pending_mshr_readback_queue(CacheConfig::MSHR_DEPTH),
      cache_size(CacheConfig::CACHE_SIZE),
      cacheline_size(CacheConfig::CACHELINE_SIZE),
      ways(CacheConfig::CACHE_WAYS),
      num_banks(CacheConfig::CACHE_BANKS),
      mshr(new MSHR(CacheConfig::MSHR_DEPTH)),
      wcb(new FixedQueue<MappedTransaction>(CacheConfig::WCB_DEPTH)),
      mp(),   // MainPipe
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

    switch (CacheConfig::pfc_cfg.type)
    {
    case CacheConfig::PrefetcherType::STREAM:
        prefetcher = new StreamPrefetcher_V2;
        break;

    case CacheConfig::PrefetcherType::SPATIAL:
        prefetcher = new SpatialPrefetcher;
        break;

    case CacheConfig::PrefetcherType::BOP:
        prefetcher = new BestOffsetPrefetcher;
        break;

    case CacheConfig::PrefetcherType::NEXT_N_LINE:
        prefetcher = new NextNLinePrefetcher;
        break;

    case CacheConfig::PrefetcherType::SPP:
        prefetcher = new SignaturePathPrefetcher;
        break;
    
    case CacheConfig::PrefetcherType::SSCAN:
        prefetcher = new SmartScanPrefetcher;
        break;
    default:
        prefetcher = nullptr;
        break;
    }
    if (prefetcher) {
        prefetcher->cache = this;
    }

    mp.cache = this;
    urq.cache = this;
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
        if (log) {
            log->in_cache(r);
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
            std::cout << std::endl << \
                "[Warn] Cache receives two trans with the same addr at the same cycle, " << \
                "but the write trans is earlier than the read trans in AXI2UI. " << \
                "This may cause consistency problem in Baiyang chisel code." << std::endl;
            if (cmdQueue.push(w) == false) {
                return NO_RW;
            }
            return WR_ONLY;
        } else {
            if (cmdQueue.push(r) == false) {
                return NO_RW;
            }
            if (log) {
                log->in_cache(r);
            }
            return RD_ONLY;
        }
    } else {
        return NO_RW;
    }
}

TIME_TYPE Cache::get_cycles()
{
    return controller->get_cycles();
}

static inline uint64_t get_bits(uint64_t data, int msb, int lsb)
{
    assert((msb >= lsb) && "MSB must be greater than or equal to LSB");
    return (data >> lsb) & ((1 << (msb - lsb + 1)) - 1);
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
    // | tag | index(idx_width) | bank(3) | 6'b0 |
    size_t idx_width = log2(num_sets);
    tag = get_bits(fake_addr, 63, idx_width + 9);
    index = get_bits(fake_addr, idx_width + 8, 9);
    bank = get_bits(fake_addr, 8, 6);
    return std::make_tuple(tag, index, bank);
};

// This function corresponds to "refillPipe" module in Baiyang chisel code.
void Cache::returnFromScg(MappedTransaction t)
{
    auto mshr_it = mshr->get_mshr_entry(t);

    // Baiyang v0.94 assumes SCG->Cache read data is not backpressured.
    // If this queue fills, MCSim has modeled an impossible cache-side stall.
    if (!t.is_prefetch) {
        bool queued = pending_mshr_readback_queue.push({t, get_cycles() + read_back_delay_from_cache_miss});
        assert(queued && "Cache backpressures SCG read data");
        if (log) {
            log->in_cache_refill(t);
        }
    }
    
    // We handle data from scg differently depending
    // on whether prefetch is enabled:
    // 1. If prefetch IS enabled, we only add data required
    //    by prefetch to the cache.
    // 2. If prefetch IS NOT enabled, we add all data to the cache.
    if (CacheConfig::pfc_cfg.ENABLE && t.is_prefetch || !CacheConfig::pfc_cfg.ENABLE) {
        // set cacheline ready
        auto [tag, index, bank] = get_tag_index_bank(t);
        size_t way = mshr_it->way;
        assert(CacheArray[bank][index][way].valid && CacheArray[bank][index][way].tag == tag);
        assert(CacheArray[bank][index][way].from_prefetch == t.is_prefetch);
        assert(CacheArray[bank][index][way].prefetch_accessed == false);
        CacheArray[bank][index][way].ready = true;
        has_refill_req = true;
        // return Data to URQ, so it can return data back to addrmap.
        tmp_urqback_queue.push({t, way});
    }

    mshr->finish_request(t);
}

bool Cache::returnDataFromURQ(MappedTransaction t, uint64_t way)
{
    // entry of URQ should always be non-prefetch cmd, return it to addrmap
    assert(t.is_read && !t.is_prefetch);
    auto [tag, index, bank] = get_tag_index_bank(t);
    assert(CacheArray[bank][index][way].valid && CacheArray[bank][index][way].tag == tag);


    if (back2am_arb.submit(ReturnToAddrMapArb::Source::URQ, t,
                           get_cycles() + read_back_delay_from_cache_hit,
                           CallerType::CACHE_HIT)) {
        stats.read_hit(t, &CacheArray[bank][index][way], get_cycles());   // ATTENTION: we count hit URQ as read_hit
        return true;
    }
    return false;
}

bool Cache::returnDataFromMP(MappedTransaction t, CallerType caller)
{
    TIME_TYPE ready_time = get_cycles() + (caller == CallerType::CACHE_MISS ? read_back_delay_from_cache_miss : read_back_delay_from_cache_hit);
    if (back2am_arb.submit(ReturnToAddrMapArb::Source::MP_S3, t, ready_time, caller)) {
        return true;
    }
    return false;
}

bool Cache::prepareReadbackCandidate(MappedTransaction t, CallerType caller, TIME_TYPE ready_time)
{
    assert(caller == CallerType::CACHE_HIT || caller == CallerType::CACHE_MISS);
    if (readback_candidate.valid) {
        return false;
    }

    readback_candidate.valid = true;
    readback_candidate.granted = false;
    readback_candidate.trans = t;
    readback_candidate.ready_time = ready_time;
    readback_candidate.caller = caller;
    return true;
}

void Cache::processPendingMSHRReadback()
{
    if (pending_mshr_readback_queue.empty()) {
        return;
    }

    const auto &entry = pending_mshr_readback_queue.front();
    if (entry.second > get_cycles()) {
        return;
    }
    if (back2am_arb.submit(ReturnToAddrMapArb::Source::MSHR, entry.first, entry.second, CallerType::CACHE_MISS)) {
        pending_mshr_readback_queue.pop();
    }
}

void Cache::drainBackToAddrMapArb()
{
    if (readback_candidate.valid) {
        return;
    }

    ReturnToAddrMapArb::Source source = back2am_arb.choose();
    if (source == ReturnToAddrMapArb::Source::NONE) {
        return;
    }

    const auto &candidate = back2am_arb.get(source);
    if (prepareReadbackCandidate(candidate.trans, candidate.caller, candidate.ready_time)) {
        back2am_arb.clear(source);
    }
}

bool Cache::hasReadyReadback() const
{
    return readback_candidate.valid && readback_candidate.ready_time <= controller->get_cycles();
}

void Cache::commitReadyReadback()
{
    assert(hasReadyReadback());
    axi2ui->put_back_ready(readback_candidate.trans.id, readback_candidate.trans.token, readback_candidate.caller);
    readback_candidate = ReadbackCandidate();
}

bool Cache::handle_forward(std::deque<uint64_t>::iterator it)
{
    MappedTransaction forward_t;
    forward_t.addr = *it;
    forward_t.is_to_cache = true;
    forward_t.is_prefetch = true;
    forward_t.is_read = true;
    forward_t.mapped_addr = linear_to_mapped(*it);

    if (mp.addTrans(forward_t)) {
        // TODO: Prefetch request may be dropped in mainPipe,
        //       so we need to dump it in mainPipe instead of here.
        // prefetch new cacheline, dump it and statistics
        dump_prefetch(*it);
        stats.prefetch();
        return true;
    } else {
        return false;
    }
}

bool Cache::handle_cmd()
{
    MappedTransaction t = cmdQueue.front();

    return mp.addTrans(t);
}

bool Cache::handle_prefetch()
{
    uint64_t addr = prefetchQueue.front();
    MappedTransaction prefetch_t;
    prefetch_t.addr = addr;
    prefetch_t.is_to_cache = true;
    prefetch_t.is_prefetch = true;
    prefetch_t.is_read = true;
    prefetch_t.mapped_addr = linear_to_mapped(addr);

    if (mp.addTrans(prefetch_t)) {
        // TODO: Prefetch request may be dropped in mainPipe,
        //       so we need to dump it in mainPipe instead of here.
        // prefetch new cacheline, dump it and statistics
        dump_prefetch(addr);
        stats.prefetch();
        return true;
    } else {
        return false;
    }
}

void Cache::mainPipe_arb()
{
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

    enum class MPArb {
        FORWARD, CMD_QUEUE, PREFETCH_QUEUE
    };
    // MPArb priority[] = {MPArb::FORWARD, MPArb::CMD_QUEUE, MPArb::PREFETCH_QUEUE};
    MPArb priority[] = {MPArb::PREFETCH_QUEUE, MPArb::CMD_QUEUE};
    // MPArb priority[] = {MPArb::CMD_QUEUE, MPArb::PREFETCH_QUEUE};
    for (int i = 0; i < std::size(priority); i++) {
        if (MPArb::FORWARD == priority[i] && (has_forward && CacheConfig::pfc_cfg.ENABLE)) {
            if (handle_forward(it)) {
                prefetchQueue.erase(it);
            }
            break;
        }
        if (MPArb::CMD_QUEUE == priority[i] && (has_cmd)) {
            if (handle_cmd()) {
                cmdQueue.pop();
            }
            break;
        }
        if (MPArb::PREFETCH_QUEUE == priority[i] && (has_prefetch && CacheConfig::pfc_cfg.ENABLE)) {
            if (handle_prefetch()) {
                prefetchQueue.pop();
            }
            break;
        }
    }
}

void Cache::writeBus_arb()
{
    // WriteBus Arb step: refillPipe has higher priority than mainPipe
    if (!has_refill_req) {
        if (mp.writeBus.valid) {
            auto &wb = mp.writeBus;
            if (CacheArray[wb.bank][wb.index][wb.way].valid && (CacheArray[wb.bank][wb.index][wb.way].tag == wb.cacheline->tag)) {
                // for hit writeback of mainPipe, ready bit shouldn't be modified
                // assert(CacheArray[wb.bank][wb.index][wb.way].ready == wb.cacheline->ready);
                
                // but if this really happened, we just ignore assignment of ready bit
                wb.cacheline->ready = CacheArray[wb.bank][wb.index][wb.way].ready;
            }
            CacheArray[wb.bank][wb.index][wb.way] = *(wb.cacheline);
            mp.writeBus.valid = false;
        }
    }
}

void Cache::step()
{
    processPendingMSHRReadback();

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
        assert(!t.is_read);
        rw_trans_type type = scheduler->addTrans(t, t, WR_ONLY, CallerType::CACHE_WRITEBACK);
        assert(type == WR_ONLY || type == NO_RW);
        if (type == WR_ONLY) {
            wcb->pop();
        }
    }

    // Prefetcher step
    if (CacheConfig::pfc_cfg.ENABLE) {
        prefetcher->step();
    }

    // ATTENTION: Sending data back to urq must be earlier than mp.step(),
    //            to gurantee urq.returnDataFromRefillPipe() is called earlier
    //            than urq.addTrans().
    if (!tmp_urqback_queue.empty()) {
        auto [t, way] = tmp_urqback_queue.front();
        if (urq.returnDataFromRefillPipe(t, way)) {
            tmp_urqback_queue.pop();
        }
    }

    // writeBus Arb step
    writeBus_arb();

    // MainPipe step
    mp.step();

    // MainPipe Arb step
    mainPipe_arb();

    // UnreadyQueue step
    urq.step();

    drainBackToAddrMapArb();

    // unset has_refill_req
    has_refill_req = false;
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

void Cache::resetStats()
{
    stats.reset();
}

bool Cache::all_done() const
{
    // TODO: Should we wait for mshr and wcb to be empty?
    //       Because some entries may be from prefetch,
    //       we don't need to wait for them.
    return cmdQueue.empty();
}
