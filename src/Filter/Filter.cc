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

#include "Filter/Filter.h"
#include "AddrMap/AddrMap.h"
rw_trans_type Filter::addTrans(const AxiTransaction &r, const AxiTransaction &w, rw_trans_type type)
{
    auto equal = [](const AxiTransaction item, AxiTransaction at) { return  item.addr == at.addr; };
    bool has_rd = type == RD_ONLY || type == RD_WR;
    bool has_wr = type == WR_ONLY || type == RD_WR;
    bool accept_rd = false, accept_wr = false;
    if (has_rd && !rTransQ.full() && !wTransQ.contains(equal, r)) {
        rTransQ.push(r);
        accept_rd = true;
        log->in_filter(r);
    }

    if (has_wr && !wTransQ.full() && !rTransQ.contains(equal, w)) {
        wTransQ.push(w);
        accept_wr = true;
    }

    if (accept_rd && accept_wr) {
        return RD_WR;
    } else if (accept_rd) {
        return RD_ONLY;
    } else if (accept_wr) {
        return WR_ONLY;
    } else {
        return NO_RW;
    }
}

bool Filter::all_done() const
{
    return rTransQ.empty() && wTransQ.empty();
}

FilterTransaction Filter::do_filter(const AxiTransaction &at)
{
    FilterTransaction ft(at);
    ft.is_prefetch = false;

    switch (filter_mode)
    {
    case FilterMode::ALL2CACHE:
        ft.is_to_cache = true;
        break;

    case FilterMode::ALL2SCHED:
        ft.is_to_cache = false;
        break;

    case FilterMode::FILTER_ADRBD:
        if (ft.addr >= adrbdl && ft.addr <= adrbdh) {
            ft.is_to_cache = true;
        } else {
            ft.is_to_cache = false;
        }
        break;

    case FilterMode::FILTER_BIT:
        // ft.is_to_cache = false;
        // for (auto bit : bits) {
        //     if ((ft.addr >> bit) & 1) {
        //         ft.is_to_cache = true;
        //     }
        // }
        if (((ft.addr >> (filter_bit-1)) & 1) == 0) {
            ft.is_to_cache = true;
        } else {
            ft.is_to_cache = false;
        }
        break;
    
    case FilterMode::FILTER_ADRBD_BIT:
        if (ft.addr >= adrbdl && ft.addr <= adrbdh \
            && ((ft.addr >> (filter_bit-1)) & 1) == 0) {
            ft.is_to_cache = true;
        } else {
            ft.is_to_cache = false;
        }
        break;

    // default ALL2SCHED
    default:
        ft.is_to_cache = false;
        break;
    }

    if (CacheConfig::CACHE_EN == false) {
        ft.is_to_cache = false;
    }

    return ft;
}

void Filter::step()
{
    // send to AddrMap
    bool has_rd = !rTransQ.empty();
    bool has_wr = !wTransQ.empty();

    rw_trans_type t;
    if (has_rd && has_wr) {
        t = RD_WR;
    } else if (has_rd) {
        t = RD_ONLY;
    } else if (has_wr) {
        t = WR_ONLY;
    } else {
        t = NO_RW;
    }

    if (t != NO_RW) {
        FilterTransaction rft = do_filter(rTransQ.front());
        FilterTransaction wft = do_filter(wTransQ.front());
        // rw_trans_type type = am->addTrans(rft, wft, t);
        rw_trans_type type = am->addTransComb(rft, wft, t);
        bool has_rd = type == RD_ONLY || type == RD_WR;
        bool has_wr = type == WR_ONLY || type == RD_WR;
        if (has_rd) {
            rTransQ.pop();
        }
        if (has_wr) {
            wTransQ.pop();
        }
    }
}