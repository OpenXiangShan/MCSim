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

AXI2UI::AXI2UI(size_t tok_cap)
    : cycles(0),
      next_valid_rd_time(0),
      next_valid_wr_time(0),
      arInTrans(Axi2uiConfig::queue_size.ar_in),
      arOutTrans(Axi2uiConfig::queue_size.ar_out),
      awInTrans(Axi2uiConfig::queue_size.aw_in),
      awOutTrans(Axi2uiConfig::queue_size.aw_out)
{
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc = new StaticVirtualChannel(tok_cap);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc = new DynamicVirtualChannel();
    }
    stats.rtrans_full = 0;
    stats.wtrans_full = 0;
}

void AXI2UI::put_back(ID_TYPE id, TOKEN_TYPE token, CallerType caller)
{
    uint64_t exp_rdback_time;
    assert(caller == CallerType::SCG || caller == CallerType::CACHE_HIT || caller == CallerType::CACHE_MISS);
    if (caller == CallerType::SCG) {
        exp_rdback_time = cycles + read_back_delay_from_scg;
    } else if (caller == CallerType::CACHE_HIT) {
        exp_rdback_time = cycles + read_back_delay_from_cache_hit;
    } else if (caller == CallerType::CACHE_MISS) {
        exp_rdback_time = cycles + read_back_delay_from_cache_miss;
    }

    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc->put_back(id, token, exp_rdback_time);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc->put_back(id, token, exp_rdback_time);
    }
}

bool AXI2UI::addTrans(SysTransaction& st)
{
    auto equal = [](const AxiTransaction item, uint64_t addr) { return  item.addr == addr; };
    if (st.is_read) {
        if (arInTrans.full()) {
            stats.rtrans_full++;
            return false;
        }
        if (awInTrans.contains(equal, st.addr)) {
            return false;
        }
    }

    if (!st.is_read) {
        if (awInTrans.full()) {
            stats.wtrans_full++;
            return false;
        }
        if (arInTrans.contains(equal, st.addr)) {
            return false;
        }
        if (cycles < next_valid_wr_time) {
            return false;
        }
    }

    // send to InTrans
    st.cycle = cycles;
    if (st.is_read) {
        arInTrans.push(st);
        log->in_axi(st);
    } else {
        awInTrans.push(st);
        next_valid_wr_time = cycles + interval;
        ia->release_wid(st.id);
    }
    return true;
}

void AXI2UI::burst_clip()
{
    static uint64_t next_rd_clip_time = 0;
    static uint64_t next_wr_clip_time = 0;
    
    bool rd_OK = true;
    bool wr_OK = true;
    if (arInTrans.empty() || arOutTrans.full() || cycles < next_rd_clip_time) {
        rd_OK = false;
    }
    if (awInTrans.empty() || awOutTrans.full() || cycles < next_wr_clip_time) {
        wr_OK = false;
    }

    if (rd_OK) {
        SysTransaction st = arInTrans.front();
        assert(st.is_read);
        AxiTransaction at(st);
        // get token
        if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
            if (!svc->addTrans(at)) {
                return;
            }
        } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
            if (!dvc->addTrans(at)) {
                return;
            }
        }
        // send to OutTrans
        assert(arOutTrans.push(std::make_pair(at, cycles + Axi2uiConfig::extra_out_latency)));
        arInTrans.pop();
        next_rd_clip_time = cycles + 2;   //TODO: for burst of any length, plus burst length
    }

    if (wr_OK) {
        SysTransaction st = awInTrans.front();
        assert(!st.is_read);
        AxiTransaction at(st);
        // send to OutTrans
        assert(awOutTrans.push(std::make_pair(at, cycles + Axi2uiConfig::extra_out_latency)));
        awInTrans.pop();
        next_wr_clip_time = cycles + 2;   //TODO: for burst of any length, plus burst length
    }
}

extern int returned;
void AXI2UI::step()
{
    cycles++;
    
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        svc->step(cycles, next_valid_rd_time, ia, log);
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        dvc->step(cycles, next_valid_rd_time, ia, log);
    }

    // send to filter
    bool rd_OK = !arOutTrans.empty() && cycles >= arOutTrans.front().second;
    bool wr_OK = !awOutTrans.empty() && cycles >= awOutTrans.front().second;
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
        auto type = ft->addTrans(arOutTrans.front().first, awOutTrans.front().first, t);
        if (type != NO_RW) {
            bool has_rd = type == RD_ONLY || type == RD_WR;
            bool has_wr = type == WR_ONLY || type == RD_WR; 
            if (has_rd) {
                arOutTrans.pop();
            }
            if (has_wr) {
                awOutTrans.pop();
            }
        }
    }

    // InTrans to OutTrans
    burst_clip();
}

TIME_TYPE AXI2UI::get_cycles() const
{
    return cycles;
}

bool AXI2UI::all_done() const
{
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        return svc->all_done();// && arInTrans.empty() && awInTrans.empty();
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        return dvc->all_done();// && arInTrans.empty() && awInTrans.empty();
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
    std::cout << "| rtrans_full: " << stats.rtrans_full << ", wtrans_full: " << stats.wtrans_full << std::endl;
    if (Axi2uiConfig::vc_mode == VcMode::STATIC) {
        std::cout << "| rob_full: " << svc->stats.rob_full << std::endl;
        std::cout << "| average_latency: " << SAFE_DIV((double)svc->stats.total_latency, (double)svc->stats.read_count) << std::endl;
    } else if (Axi2uiConfig::vc_mode == VcMode::DYNAMIC) {
        std::cout << "| rob_full: " << dvc->stats.rob_full \
            << ", max_rob_size: " << dvc->stats.max_rob_size << std::endl;
        std::cout << "| average_latency: " << SAFE_DIV((double)dvc->stats.total_latency, (double)dvc->stats.read_count) << std::endl;
    }
}