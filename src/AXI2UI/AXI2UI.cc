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

#include "AXI2UI/AXI2UI.h"
#include "AXI2UI/VirtualChannel.h"
#include "Filter/Filter.h"
#include "config.h"
#include "utils/debug.h"
#include "utils/log.h"
#include "top/Driver.h"
#include <cassert>
#define SAFE_DIV(a,b) (((b) == 0) ? 0 : ((a)/(b)))

AXI2UI::AXI2UI(const uint64_t &cycles, size_t tok_cap, int channel_id)
    : cycles(cycles),
      channel_id(channel_id),
      next_valid_rd_time(0),
      arInTrans(Axi2uiConfig::queue_size.ar_in),
      arOutTrans(Axi2uiConfig::queue_size.ar_out),
      awInTrans(Axi2uiConfig::queue_size.aw_in),
      awOutTrans(Axi2uiConfig::queue_size.aw_out),
      BChannelTrans(16)
{
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc = new StaticVirtualChannel(tok_cap);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc = new DynamicVirtualChannel();
    }
    stats.rtrans_full = 0;
    stats.wtrans_full = 0;
    stats.read_count = 0;
    stats.write_count = 0;
}

bool AXI2UI::rangesOverlap(uint64_t lhs_addr, uint64_t rhs_addr)
{
    uint64_t lhs_end = lhs_addr + UI_TRANSACTION_BYTES;
    uint64_t rhs_end = rhs_addr + UI_TRANSACTION_BYTES;
    return lhs_addr < rhs_end && rhs_addr < lhs_end;
}

bool AXI2UI::hasPendingReadConflict(uint64_t addr) const
{
    for (const auto &item : arInTrans) {
        if (rangesOverlap(item.addr, addr)) {
            return true;
        }
    }
    for (const auto &item : arOutTrans) {
        if (rangesOverlap(item.first.addr, addr)) {
            return true;
        }
    }
    return false;
}

bool AXI2UI::hasPendingWriteConflict(uint64_t addr) const
{
    for (const auto &item : awInTrans) {
        if (rangesOverlap(item.addr, addr)) {
            return true;
        }
    }
    for (const auto &item : awOutTrans) {
        if (rangesOverlap(item.first.addr, addr)) {
            return true;
        }
    }
    return false;
}

bool AXI2UI::prepareReadForFilter(AxiTransaction &at)
{
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        return svc->peekToken(at);
    }
    return false;
}

bool AXI2UI::commitReadToVc(AxiTransaction &at)
{
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        return svc->addTrans(at);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        return dvc->addTrans(at);
    }
    return true;
}

void AXI2UI::put_back(ID_TYPE id, TOKEN_TYPE token, CallerType caller)
{
    TIME_TYPE exp_rdback_time;
    assert(caller == CallerType::SCG || caller == CallerType::CACHE_HIT || caller == CallerType::CACHE_MISS);
    if (caller == CallerType::SCG) {
        exp_rdback_time = cycles + scg_dfi_data_to_rob_delay;
    } else if (caller == CallerType::CACHE_HIT) {
        exp_rdback_time = cycles + read_back_delay_from_cache_hit;
    } else if (caller == CallerType::CACHE_MISS) {
        exp_rdback_time = cycles + read_back_delay_from_cache_miss;
    }

    put_back_at(id, token, exp_rdback_time, caller);
}

void AXI2UI::put_back_at(ID_TYPE id, TOKEN_TYPE token, TIME_TYPE exp_rdback_time, CallerType caller)
{
    assert(caller == CallerType::SCG || caller == CallerType::CACHE_HIT || caller == CallerType::CACHE_MISS);

    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc->put_back(id, token, exp_rdback_time, caller);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc->put_back(id, token, exp_rdback_time, caller);
    }

    if (log) {
        log->rob_in_at(id, token, caller, exp_rdback_time);
    }
}

void AXI2UI::put_back_ready(ID_TYPE id, TOKEN_TYPE token, CallerType caller)
{
    put_back_at(id, token, cycles, caller);
}

bool AXI2UI::addTrans(SysTransaction st)
{
    if (st.is_read) {
        if (arInTrans.full()) {
            stats.rtrans_full++;
            return false;
        }
        if (hasPendingWriteConflict(st.addr)) {
            return false;
        }
    }

    if (!st.is_read) {
        if (awInTrans.full() || BChannelTrans.full()) {
            stats.wtrans_full++;
            return false;
        }
        if (hasPendingReadConflict(st.addr)) {
            return false;
        }
    }

    // send to InTrans (need to copy because st is const ref)
    st.cycle = cycles;
    if (st.is_read) {
        arInTrans.push(st);
        stats.read_count++;
        log->in_axi(st);
    } else {
        awInTrans.push(st);
        stats.write_count++;
        // In v0.94, B is ordered by accepted AW/W-last, independent of UI write completion.
        assert(BChannelTrans.push(st.id));
    }
    return true;
}

bool AXI2UI::willAccept(uint64_t addr, bool is_read) const
{
    if (is_read) {
        return !arInTrans.full() && !hasPendingWriteConflict(addr);
    }
    return !awInTrans.full() && !BChannelTrans.full() &&
           !hasPendingReadConflict(addr);
}

void AXI2UI::burst_clip()
{
    bool rd_OK = !arInTrans.empty() && !arOutTrans.full();
    bool wr_OK = !awInTrans.empty() && !awOutTrans.full();

    if (rd_OK) {
        SysTransaction st = arInTrans.front();
        assert(st.is_read);
        AxiTransaction at(st);
        assert(arOutTrans.push(std::make_pair(at, cycles)));
        arInTrans.pop();
    }

    if (wr_OK) {
        SysTransaction st = awInTrans.front();
        assert(!st.is_read);
        AxiTransaction at(st);
        assert(awOutTrans.push(std::make_pair(at, cycles)));
        awInTrans.pop();
    }
}

extern int returned;
void AXI2UI::step()
{
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc->step(cycles, next_valid_rd_time, ia, log, channel_id);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc->step(cycles, next_valid_rd_time, ia, log, channel_id);
    }

    // send to filter
    bool rd_OK = !arOutTrans.empty() && cycles >= arOutTrans.front().second;
    bool wr_OK = !awOutTrans.empty() && cycles >= awOutTrans.front().second;
    AxiTransaction r = rd_OK ? arOutTrans.front().first : AxiTransaction();
    AxiTransaction w = wr_OK ? awOutTrans.front().first : AxiTransaction();
    if (rd_OK && !prepareReadForFilter(r)) {
        rd_OK = false;
        r = AxiTransaction();
    }
    rw_trans_type t;
    if (rd_OK && wr_OK) {
        t = RD_WR;
    } else if (rd_OK) {
        t = RD_ONLY;
    } else if (wr_OK) {
        t = WR_ONLY;
    } else {
        t = NO_RW;
    }

    if (t != NO_RW) {
        auto type = ft->addTrans(r, w, t);
        if (type != NO_RW) {
            bool has_rd = type == RD_ONLY || type == RD_WR;
            bool has_wr = type == WR_ONLY || type == RD_WR; 
            if (has_rd) {
                assert(commitReadToVc(r));
                arOutTrans.pop();
            }
            if (has_wr) {
                awOutTrans.pop();
            }
        }
    }

    // InTrans to OutTrans
    burst_clip();

    // B channel response
    if (!BChannelTrans.empty()) {
        ia->release_wid(BChannelTrans.front(), channel_id);
        BChannelTrans.pop();
    }
}

bool AXI2UI::all_done() const
{
    bool vc_done = false;
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        vc_done = svc->all_done();
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        vc_done = dvc->all_done();
    }
    return vc_done && arInTrans.empty() && arOutTrans.empty() && awInTrans.empty() \
        && awOutTrans.empty() && BChannelTrans.empty();
}

void AXI2UI::print_latency_distribution() const
{
    if (!DumpConfig::DUMP_LATENCY) {
        return;
    }

    const std::map<int, int>* latency_dist = nullptr;
    int granularity = 0;
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        latency_dist = &(svc->stats.latency_distribution);
        granularity = svc->stats.granularity;
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        latency_dist = &(dvc->stats.latency_distribution);
        granularity = dvc->stats.granularity;
    }

    if (!latency_dist) {
        std::cout << "No latency distribution gotten." << std::endl;
        return;
    }

    if (granularity <= 0) {
        std::cout << "Granularity of latency distribution is not correctly set." << std::endl;
        return;
    }

    std::cout << "=============== Latency Distribution ===============" << std::endl;
    std::cout << "| Granularity: " << granularity << " cycles" << std::endl;
    std::cout << "| " << std::left << std::setw(12) << "Range" << "Count" << std::endl;
    std::cout << "| " << std::left << std::setw(12) << "------------" << "------------" << std::endl;

    for (const auto& entry : *latency_dist) {
        int bucket = entry.first;
        int count = entry.second;
        int start = bucket * granularity;
        int end = (bucket + 1) * granularity - 1;

        std::cout << "| " << std::left << std::setw(12) << (std::to_string(start) + "-" + std::to_string(end)) << count << std::endl;
    }
}

void AXI2UI::resetStats()
{
    stats.rtrans_full = 0;
    stats.wtrans_full = 0;
    stats.read_count = 0;
    stats.write_count = 0;

    // 清零 VirtualChannel 中的 latency distribution
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc->stats.latency_distribution.clear();
        svc->stats.total_latency = 0;
        svc->stats.read_count = 0;
        svc->stats.rob_full = 0;
        svc->stats.cache_read_hit_count = 0;
        svc->stats.total_cache_hit_latency = 0;
        svc->stats.cache_read_miss_count = 0;
        svc->stats.total_cache_miss_latency = 0;
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc->stats.latency_distribution.clear();
        dvc->stats.total_latency = 0;
        dvc->stats.read_count = 0;
        dvc->stats.rob_full = 0;
        dvc->stats.max_rob_size = 0;
    }
}

void AXI2UI::statistics() const
{
    std::cout << "================ AXI2UI STATISTICS ==================" << std::endl;
    // std::cout << "rd_latency" << std::endl;
    // for (auto &vc: vcs) {
    //     for (auto &item: vc->rd_latency) {
    //         std::cout << item.first << " " << item.second << std::endl;
    //     }
    // }
    std::cout << "| Read requests: " << stats.read_count << ", Write requests: " << stats.write_count << std::endl;
    std::cout << "| rtrans_full: " << stats.rtrans_full << ", wtrans_full: " << stats.wtrans_full << std::endl;
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        std::cout << "| rob_full: " << svc->stats.rob_full << std::endl;
        std::cout << "| average_latency: " << SAFE_DIV((double)svc->stats.total_latency, (double)svc->stats.read_count) << std::endl;
        std::cout << "| average_cache_hit_latency: " << SAFE_DIV((double)svc->stats.total_cache_hit_latency, (double)svc->stats.cache_read_hit_count) << std::endl;
        std::cout << "| average_cache_miss_latency: " << SAFE_DIV((double)svc->stats.total_cache_miss_latency, (double)svc->stats.cache_read_miss_count) << std::endl;

    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        std::cout << "| rob_full: " << dvc->stats.rob_full \
            << ", max_rob_size: " << dvc->stats.max_rob_size << std::endl;
        std::cout << "| average_latency: " << SAFE_DIV((double)dvc->stats.total_latency, (double)dvc->stats.read_count) << std::endl;
    }
    print_latency_distribution();
}
