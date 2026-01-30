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

#include "Cache/Prefetcher.h"
#include "Cache/Cache.h"
#include <assert.h>

Prefetcher::Prefetcher()
    : state(IDLE),
      s1_input_queue(QueueDepth)
{
    // init ghb
    for (auto &entry : ghb) {
        entry.valid = false;
        entry.addr = 0;
        for (auto &stride : entry.strides) {
            stride.valid = false;
            stride.stride = 0;
            stride.count = 0;
        }
    }
    // init s4_out_queue
    for (int i = 0; i < cfg.MAX_STREAMS; i++) {
        s4_output_queue[i] = new FixedQueue<uint64_t>(QueueDepth);
    }
}


void Prefetcher::s1_init()
{
    s4.need_prefetch = false;
    if (ghb[0].valid) {
        for (auto &stride_pair : ghb[0].strides) {
            if (stride_pair.count > cfg.PREFETCH_THRESHOLD) {
                s4.need_prefetch = true;
            }
        }
    }
    s1.miss_valid = !s1_input_queue.empty() && state == IDLE;
}

void Prefetcher::s2_init()
{
    s2.stride_count = 0;
}

void Prefetcher::s2_to_s3_init()
{
    s3.fini = false;
    s3.index = 0;
}

void Prefetcher::s1_to_s4_init()
{
    s4.fini = false;
    s4.index = 0;
}

void Prefetcher::update_ghb()
{
    assert(!s1_input_queue.empty());
    for (int i = ghb.size() - 1; i > 0; i--) {
        ghb[i] = ghb[i - 1];
    }
    ghb[0].valid = true;
    ghb[0].addr = s1_input_queue.front();
    for (auto stride : ghb[0].strides) {
        stride.valid = false;
        stride.stride = 0;
        stride.count = 0;
    }
    s1_input_queue.pop();
}

void Prefetcher::cal_stride()
{
    assert(ghb[0].valid);
    assert(s2.stride_count == 0);
    std::array<uint64_t, cfg.GHB_NUM_ENTRIES> stride_i;

    auto match_i = [](Prefetcher::GhbEntry::GhbEntryStridePair &stride_pair, uint64_t i) {
        return stride_pair.valid && (stride_pair.stride == i) \
            && (stride_pair.stride != 0) && stride_pair.count > 0;
    };
    auto again_i = [](Prefetcher::GhbEntry::GhbEntryStridePair &stride_pair, uint64_t i) {
        return stride_pair.valid && (stride_pair.stride == i) \
            && (stride_pair.stride == 0) && stride_pair.count > 0;
    };

    // calculate stride
    for (int i = 1; i < cfg.GHB_NUM_ENTRIES; i++) {
        if (!ghb[i].valid) {
            continue;
        }
        stride_i[i] = ghb[0].addr - ghb[i].addr;
    }

    // (succeed old strides)
    // set all matched strides in stride_i by the following order:
    // 1. stride != 0;
    // 2. stride == 0; (which is stream of the same addr)
    for (int i = 1; i < cfg.GHB_NUM_ENTRIES; i++) {
        if (!ghb[i].valid) {
            continue;
        }
        if (s2.stride_count >= cfg.MAX_STREAMS) {
            return;
        }
        for (auto stride_pair : ghb[i].strides) {
            if (match_i(stride_pair, stride_i[i])) {
                ghb[0].strides[s2.stride_count].valid = true;
                ghb[0].strides[s2.stride_count].stride = stride_pair.stride;
                ghb[0].strides[s2.stride_count].count = stride_pair.count + 1;
                s2.stride_count++;
            }
        }
    }
    for (int i = 1; i < cfg.GHB_NUM_ENTRIES; i++) {
        if (!ghb[i].valid) {
            continue;
        }
        if (s2.stride_count >= cfg.MAX_STREAMS) {
            return;
        }
        for (auto stride_pair : ghb[i].strides) {
            if (again_i(stride_pair, stride_i[i])) {
                ghb[0].strides[s2.stride_count].valid = true;
                ghb[0].strides[s2.stride_count].stride = stride_pair.stride;
                ghb[0].strides[s2.stride_count].count = stride_pair.count;
                s2.stride_count++;
            }
        }
    }

    // (create new strides)
    // if not all MAX_STREAMS strides in the new GHB entry have been set,
    // we check if there is new stride, and set it
    for (int i = 1; i < cfg.GHB_NUM_ENTRIES; i++) {
        if (!ghb[i].valid) {
            continue;
        }
        if (s2.stride_count >= cfg.MAX_STREAMS) {
            return;
        }
        for (int j = i + 1; j < cfg.GHB_NUM_ENTRIES; j++) {
            if (!ghb[j].valid) {
                continue;
            }
            uint64_t stride_ij = ghb[i].addr - ghb[j].addr;
            // check if there is the same stride in ghb[0] (have created)
            bool created = false;
            for (auto &stride_pair : ghb[0].strides) {
                if (stride_pair.valid && (stride_pair.stride == stride_ij)) {
                    created = true;
                    break;
                }
            }
            if (created) {
                continue;
            }
            // TODO: should we track stream with stride of zero?
            if (stride_ij == stride_i[i]) {
                ghb[0].strides[s2.stride_count].valid = true;
                ghb[0].strides[s2.stride_count].stride = stride_ij;
                ghb[0].strides[s2.stride_count].count = 1;
                s2.stride_count++;
            }
        }
    }
}

void Prefetcher::update_stride()
{
    // we have been updated stride in s2(cal_stride),
    // so we only need to wait for s2.stride_count cycles in s3(update_stride)
    assert(s3.index <= cfg.MAX_STREAMS);
    s3.fini = s3.index >= cfg.MAX_STREAMS;
    s3.index++;
}

bool Prefetcher::is_prefetched(uint64_t addr)
{
    bool addr_exists = false;
    for (auto &queue : s4_output_queue) {
        if (queue->contains(std::equal_to<uint64_t>(), addr)) {
            addr_exists = true;
            break;
        }
    }
    return addr_exists;
}

// generate prefetch request
void Prefetcher::do_prefetch()
{
    assert(ghb[0].valid);
    s4.fini = s4.index >= cfg.PREFETCH_NUM;
    if (s4.fini) {
        return;
    }

    static uint64_t s4_addr[cfg.MAX_STREAMS];

    for (int i = 0; i < cfg.MAX_STREAMS; i++) {
        GhbEntry::GhbEntryStridePair &stride_pair = ghb[0].strides[i];
        if (s4.index == 0) {
            s4_addr[i] = ghb[0].addr + stride_pair.stride;
        } else {
            s4_addr[i] = s4_addr[i] + stride_pair.stride;
        }
        if (stride_pair.valid && stride_pair.count >= cfg.PREFETCH_THRESHOLD) {
            // last cycle, set count to zero
            if (s4.index == cfg.PREFETCH_NUM - 1) {
                stride_pair.count = 0;
            }
            if (s4_output_queue[i]->full()) {
                // for simplify, we abort this stream
                // TODO: should we abort this stream?
                continue;
            }
            if (!is_prefetched(s4_addr[i])) {
                s4_output_queue[i]->push(s4_addr[i]);
            }
        }
    }

    s4.index++;
}

// RoundRobin Arbiter step
void Prefetcher::s4_rr_arb()
{
    static int pointer = 0;
    for (int i = 1; i <= cfg.MAX_STREAMS; i++) {
        int idx = (pointer + i) % cfg.MAX_STREAMS;
        if (!s4_output_queue[idx]->empty()) {
            uint64_t addr = s4_output_queue[idx]->front() * CacheConfig::CACHELINE_SIZE;
            if (cache->addPrefetch(addr)) {
                s4_output_queue[idx]->pop();
                pointer = idx;
                break;
            }
        }
    }
}

void Prefetcher::step()
{
    s4_rr_arb();

    // FSM
    switch (state) {
        // s1
        case IDLE:
            s1_init();
            if (s4.need_prefetch) {
                s1_to_s4_init();
                state = PREFETCH;
            } else if (s1.miss_valid) {
                update_ghb();
                state = CAL_STRIDE;
            }
            break;
        // s2
        case CAL_STRIDE:
            s2_init();
            cal_stride();
            s2_to_s3_init();
            state = UPDATE_STRIDE;
            break;
        // s3
        case UPDATE_STRIDE:
            update_stride();
            if (s3.fini) {
                state = IDLE;
            }
            break;
        // s4
        case PREFETCH:
            do_prefetch();
            if (s4.fini) {
                state = IDLE;
            }
            break;

        default:
            break;
    }
}

bool Prefetcher::addTrans(const MappedTransaction &t)
{
    // TODO: Baiyang rtl suppose s1_in_queue is large enough
    // We always need the latest addr,
    // so when the input queue is full,
    // we abort the oldest one.
    if (s1_input_queue.full()) {
        s1_input_queue.pop();
    }

    s1_input_queue.push(t.addr / CacheConfig::CACHELINE_SIZE);   // cacheline addr
    return true;
}
