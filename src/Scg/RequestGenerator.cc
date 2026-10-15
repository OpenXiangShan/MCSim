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

#include "Scg/RequestGenrator.h"
#include "Scg/SdramCommandGen.h"
#include "top/Controller.h"
#include "utils/debug.h"
#include <iostream>
#include <assert.h>
RequestGenerator::RequestGenerator(): transQ(1), state(QUERY_PAGE), block_req(false), release_req(false),
    act_resp(false), pre_resp(false), pre_req(false), act_req(false), cas_req(false), cas_resp(false), p({false, 0}) {}

void RequestGenerator::set_block()
{
    block_req = true; release_req = false;
}

void RequestGenerator::set_release()
{
    block_req = false; release_req = true;
}

bool RequestGenerator::addTrans(const MappedTransaction &trans)
{
    return transQ.push(trans);
}

bool RequestGenerator::blocked() const
{
    return state == BLOCKED;
}

bool RequestGenerator::should_close()
{
    if (!p.open) {
        return false;
    }
    
    if (scg->scheduler->has_page_hit(bank_id, p.row)) {
        return false;
    }

    if (RowPolicy::CLOSE == ScgConfig::row_policy && close_row_timer >= ScgConfig::close_row_time) {
        return true;
    } else if (RowPolicy::DYNAMIC == ScgConfig::row_policy) {
        if (dr_prof.should_close && close_row_timer >= ScgConfig::close_row_time) {
            return true;
        }
    }

    return false;
}

void RequestGenerator::DY_PROF::step(SdramCommandGen* scg, RequestGenerator* rg)
{
    assert(t < T);
    TIME_TYPE cycle = scg->controller->get_cycles();

    p_i = cycle / T;
    if (cycle == p_i * T) {
        // init this period
        last_row_hit = rg->stats.row_hit_cnt;
        last_row_miss = rg->stats.row_miss_cnt;
        should_close = false;
    } else if (cycle < p_i * T + t) {
        // profiling
        // Because we assume this function is called every cycle,
        // we only need to calculate when cycle = p_i * T + t,
        // so do nothing here.
    } else if (cycle == p_i * T + t) {
        // profiling
        period_row_hit = rg->stats.row_hit_cnt - last_row_hit;
        period_row_miss = rg->stats.row_miss_cnt - last_row_miss;
    } else if (cycle == p_i * T + t + 1) {
        // get result
        uint64_t total_access = period_row_hit + period_row_miss;
        if (total_access == 0) {
            should_close = false;
        } else {
            float hit_rate = (float)period_row_hit / total_access;
            if (hit_rate < R) {
                should_close = true;
            }
        }
    }
}

void RequestGenerator::step()
{
// dynamic row profiling
    dr_prof.step(scg, this);

// state machine
    switch (state) {
        case (QUERY_PAGE):
            if (block_req) {
                block_req = false;
                state = BLOCKED;
            } else if (!transQ.empty()) {
                auto cur = transQ.front();
                if (p.open) {
                    if (p.row == cur.mapped_addr.row) {
                        state = CAS;
                        cas_req = true;
                        cas_resp = false;
                        stats.row_hit();
                    } else { // different row, close page first
                        p.open = false;
                        state = PRE;
                        pre_req = true;
                        pre_resp = false;
                        stats.row_miss();
                    }
                } else { // closed page
                    p.row = cur.mapped_addr.row;
                    p.open = true;
                    state = ACT;
                    act_req = true;
                    act_resp = false;
                    stats.row_miss();
                }
            } else if (should_close()) {
                if (p.open) {
                    p.open = false;
                    state = PRE;
                    pre_req = true;
                    pre_resp = false;
                }
            }
            break;
        case BLOCKED:
            if (release_req) {
                state = QUERY_PAGE;
                // close all pages
                p.open = false;
            }
            break;
        case PRE:
            if (block_req) {
                block_req = false;
                pre_req = false;
                state = BLOCKED;
            } else if (pre_resp) {
                pre_req = false;
                state = QUERY_PAGE;
            }
            break;
        case ACT:
            if (block_req) {
                block_req = false;
                act_req = false;
                state = BLOCKED;
            } else if (act_resp) {
                act_req = false;
                state = CAS;
                cas_req = true;
                cas_resp = false;
            }
            break;
        case CAS:
            if (cas_resp) {
                cas_req = false;
                state = QUERY_PAGE;
                // std::cout << transQ.front().is_read << std::endl;
                stats.cas_finish();
                transQ.pop();
                if (should_close()) {
                    assert(p.open);
                    // We can send certain CAS cmd so that the row be closed immediately after CAS,
                    // so no need to send PRE later.
                    // However, we still need to wait for timing check, so jump to PRE state here.
                    p.open = false;
                    state = PRE;
                    pre_req = true;
                    pre_resp = false;
                }
            }
            break;
    }
    close_row_timer = transQ.empty() ? close_row_timer + 1 : 0;
}