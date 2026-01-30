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

#include "AddrMap/AddrMap.h"
#include "utils/debug.h"
#include "Scheduler/TransactionSchedule.h"
#include "AXI2UI/AXI2UI.h" // Add this include to define reorder_buffer

MappedAddr linear_to_mapped(uint64_t addr)
{
    MappedAddr mapped;
    mapped.row = 0;
    // for (int i = 0; i < 15; ++i) {
        // mapped.row |= ((addr >> row_bits[i]) & 1) << (14 - i);
    // }
    // for (int i = 0; i < 7; ++i) {
    //     mapped.col |= ((addr >> col_bits[i]) & 1) << (6 - i);
    // }
    // for (int i = 0; i < 2; ++i) {
    //     mapped.bank |= ((addr >> bank_bits[i]) & 1) << (1 - i);
    // }
    // for (int i = 0; i < 2; ++i) {
    //     mapped.bg |= ((addr >> bg_bits[i]) & 1) << (1 - i);
    // }

    // | row | col | bank | bg |
    // mapped.bg = (addr >> 6) & 0x3;
    // mapped.bank = (addr >> 8) & 0x3;
    // mapped.col = (addr >> 10) & 0x7f;
    // mapped.row = (addr >> 17) & 0x7fff;

    // | bank | row | col | bg |
    // mapped.bg = (addr >> 6) & 0x3;
    // mapped.col = (addr >> 8) & 0x7f;
    // mapped.row = (addr >> 15) & 0x7fff;
    // mapped.bank = (addr >> 30) & 0x3;

    // | row | bank | col | bg |
    // mapped.bg = (addr >> 6) & 0x3;
    // mapped.col = (addr >> 8) & 0x7f;
    // mapped.bank = (addr >> 15) & 0x3;
    // mapped.row = (addr >> 17) & 0x7fff;

    // | row | col2 | bank | col5 | bg |
    // mapped.bg = (addr >> 6) & 0x3;
    // mapped.col = ((addr >> 8) & 0x1f) | (((addr >> 15) & 0x3) << 5);
    // mapped.bank = (addr >> 13) & 0x3;
    // mapped.row = (addr >> 17) & 0x7fff;

    // | row | col7 | bank | bg | col3 |
    // mapped.col = (((addr >> 10) & 0x7f) << 3) | ((addr >> 3) & 0x7);
    // mapped.bg = (addr >> 6) & 0x3;
    // mapped.bank = (addr >> 8) & 0x3;
    // mapped.row = (addr >> 17) & 0xffff;
    // mapped.rank = (addr >> 33) & 0x1;
    // return mapped;

    // | rank | row | bank | col7 | bg | col3 |
    mapped.col = (((addr >> 8) & 0x7f) << 3) | ((addr >> 3) & 0x7);
    mapped.bg = (addr >> 6) & 0x3;
    mapped.bank = (addr >> 15) & 0x3;
    mapped.row = (addr >> 17) & 0xffff;
    mapped.rank = (addr >> 33) & 0x1;
    return mapped;
}

rw_trans_type AddrMap::addTrans(const FilterTransaction &r, const FilterTransaction &w, rw_trans_type type)
{
    auto equal = [](const MappedTransaction item, FilterTransaction ft) { return  item.addr == ft.addr; };
    MappedTransaction rmt(r), wmt(w);
    bool has_rd = type == RD_ONLY || type == RD_WR;
    bool has_wr = type == WR_ONLY || type == RD_WR;
    bool accept_rd = false, accept_wr = false;
    if (has_rd && !rTrans.full() && !wTrans.contains(equal, r)) {
        rmt.mapped_addr = linear_to_mapped(r.addr);
        // std::cout << "[AddrMap]" << " token" << rmt.token << std::endl;
        debug("token %d\n", rmt.token);
        rTrans.push(rmt);
        accept_rd = true;
        // std::cout << "addr" << std::hex << r.addr << " mapped_addr:" << rmt.mapped_addr << std::endl;
        debug("addr %lx mapped_addr %lx\n", r.addr, rmt.mapped_addr);
        log->in_addrmap(r);
    }

    if (has_wr && !wTrans.full() && !rTrans.contains(equal, w)) {
        wmt.mapped_addr = linear_to_mapped(w.addr);
        wTrans.push(wmt);
        accept_wr = true;
        // std::cout << "addr" << std::hex << w.addr << " mapped_addr:" << wmt.mapped_addr << std::endl;
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

void AddrMap::step()
{
    // send to Scheduler
    bool has_rd = !rTrans.empty();
    bool has_wr = !wTrans.empty();

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

    bool rd_success = false;
    bool wr_success = false;
    if (t != NO_RW) {
        MappedTransaction r = rTrans.front();
        MappedTransaction w = wTrans.front();
        rw_trans_type rtype, wtype;
        switch (t)
        {
        case RD_WR:
            if (r.is_to_cache && w.is_to_cache) {
                rtype = cache->addTrans(r, w, t);
                wtype = rtype;
            } else if (!r.is_to_cache && !w.is_to_cache) {
                rtype = scheduler->addTrans(r, w, t, CallerType::ADDRMAP);
                wtype = rtype;
            } else if (r.is_to_cache) {
                rtype = cache->addTrans(r, w, RD_ONLY);
                wtype = scheduler->addTrans(r, w, WR_ONLY, CallerType::ADDRMAP);
            } else {
                wtype = cache->addTrans(r, w, WR_ONLY);
                rtype = scheduler->addTrans(r, w, RD_ONLY, CallerType::ADDRMAP);
            }
            rd_success = rtype == RD_ONLY || rtype == RD_WR;
            wr_success = wtype == WR_ONLY || wtype == RD_WR;
            break;
        
        case RD_ONLY:
            if (r.is_to_cache) {
                rtype = cache->addTrans(r, w, t);
            } else {
                rtype = scheduler->addTrans(r, w, t, CallerType::ADDRMAP);
            }
            rd_success = rtype == RD_ONLY || rtype == RD_WR;
            break;
        
        case WR_ONLY:
            if (w.is_to_cache) {
                wtype = cache->addTrans(r, w, t);
            } else {
                wtype = scheduler->addTrans(r, w, t, CallerType::ADDRMAP);
            }
            wr_success = wtype == WR_ONLY || wtype == RD_WR;
            break;
        
        default:
            break;
        }
        if (rd_success) {
            rTrans.pop();
        }
        if (wr_success) {
            wTrans.pop();
        }
    }
}

bool AddrMap::all_done() const
{
    return rTrans.empty() && wTrans.empty();
}

rw_trans_type AddrMap::addTransComb(const FilterTransaction &r, const FilterTransaction &w, rw_trans_type type)
{
    MappedTransaction rmt(r), wmt(w);

    bool rd_success = false;
    bool wr_success = false;
    if (type != NO_RW) {
        rw_trans_type rtype, wtype;
        switch (type)
        {
        case RD_WR:
            rmt.mapped_addr = linear_to_mapped(r.addr);
            wmt.mapped_addr = linear_to_mapped(w.addr);
            if (rmt.is_to_cache && wmt.is_to_cache) {
                rtype = cache->addTrans(rmt, wmt, type);
                wtype = rtype;
            } else if (!rmt.is_to_cache && !wmt.is_to_cache) {
                rtype = scheduler->addTrans(rmt, wmt, type, CallerType::ADDRMAP);
                wtype = rtype;
            } else if (rmt.is_to_cache) {
                rtype = cache->addTrans(rmt, wmt, RD_ONLY);
                wtype = scheduler->addTrans(rmt, wmt, WR_ONLY, CallerType::ADDRMAP);
            } else {
                wtype = cache->addTrans(rmt, wmt, WR_ONLY);
                rtype = scheduler->addTrans(rmt, wmt, RD_ONLY, CallerType::ADDRMAP);
            }
            rd_success = rtype == RD_ONLY || rtype == RD_WR;
            wr_success = wtype == WR_ONLY || wtype == RD_WR;
            break;
        
        case RD_ONLY:
            rmt.mapped_addr = linear_to_mapped(r.addr);
            if (rmt.is_to_cache) {
                rtype = cache->addTrans(rmt, wmt, type);
            } else {
                rtype = scheduler->addTrans(rmt, wmt, type, CallerType::ADDRMAP);
            }
            rd_success = rtype == RD_ONLY || rtype == RD_WR;
            break;
        
        case WR_ONLY:
            wmt.mapped_addr = linear_to_mapped(w.addr);
            if (wmt.is_to_cache) {
                wtype = cache->addTrans(rmt, wmt, type);
            } else {
                wtype = scheduler->addTrans(rmt, wmt, type, CallerType::ADDRMAP);
            }
            wr_success = wtype == WR_ONLY || wtype == RD_WR;
            break;
        
        default:
            break;
        }
    }

    if (rd_success) {
        log->in_addrmap(rmt);
    }

    if (rd_success && wr_success) {
        return RD_WR;
    } else if (rd_success && !wr_success) {
        return RD_ONLY;
    } else if (!rd_success && wr_success) {
        return WR_ONLY;
    } else {
        return NO_RW;
    }
}