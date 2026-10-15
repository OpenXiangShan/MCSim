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

#include "Cache/UnreadyQueue.h"
#include "Cache/Cache.h"
#include <assert.h>

// ATTENTION: addTrans() and returnDataFromRefillPipe() are called by other modules.
//            They should gurantee that returnDataFromRefillPipe() is called earlier
//            than addTrans(), otherwise is_forward info is not correct.
bool UnreadyQueue::addTrans(MappedTransaction t, uint64_t way)
{
    mp_info.has_req = true;
    mp_info.fire = false;
    mp_info.t = t;
    mp_info.way = way;

    update_step_info();
    if (!step_info.is_forward) {
        // highest invalid entry
        auto it = std::find_if(URQueue[way].rbegin(), URQueue[way].rend(),   // (rbegin, rend): find highest invalid entry
            [](const URQEntry& entry) { return entry.valid == false; });
        
        if (it != URQueue[way].rend()) {
            it->valid = true;
            it->t = t;
            mp_info.fire = true;
        }
    } else {
        // forward, directly return to addrmap
        assert(mp_info.t.addr == rf_info.t.addr);
        if (cache->returnDataFromURQ(t, way)) {
            mp_info.fire = true;
        }
    }

    return mp_info.fire;
}

void UnreadyQueue::S2_check_and_return()
{
    if (!data_back_info.fire) {
        auto [rf_tag, rf_index, rf_bank] = cache->get_tag_index_bank(rf_info.t);
        // check if RFHit
        for (auto&entry : URQueue[rf_info.way]) {
            if (is_matching_entry(entry, rf_index, rf_bank)) {
                rf_info.rf_hit = true;
                // return it to addrmap
                if (cache->returnDataFromURQ(entry.t, rf_info.way)) {
                    entry.valid = false;
                }
                return;
            }
        }
        data_back_info.fire = true;
    }
}

bool UnreadyQueue::returnDataFromRefillPipe(MappedTransaction t, uint64_t way)
{
    if (!(state == S1)) {
        return false;
    }

    rf_info.has_req = true;
    rf_info.fire = true;
    rf_info.t = t;
    rf_info.way = way;

    S2_check_and_return();

    update_step_info();
    return true;
}

void UnreadyQueue::reset_info()
{
    if (mp_info.has_req && mp_info.fire) {
        mp_info.has_req = false;
        mp_info.fire = false;
    }
    if (rf_info.has_req && rf_info.fire) {
        rf_info.has_req = false;
        rf_info.fire = false;
        rf_info.rf_hit = false;
    }
    // Because we assume AXI2UI.data_back.ready is always true, so always fire.
    // Unset fire here.
    data_back_info.fire = false;
}

void UnreadyQueue::update_step_info()
{
    auto [rf_tag, rf_index, rf_bank] = cache->get_tag_index_bank(rf_info.t);
    auto [mp_tag, mp_index, mp_bank] = cache->get_tag_index_bank(mp_info.t);

    step_info.is_forward = (state == S1 && mp_info.has_req && rf_info.has_req) && \
        (rf_info.way == mp_info.way) && (rf_bank == mp_bank) && (rf_index == mp_index);
}

void UnreadyQueue::step()
{
    update_step_info();
    // FSM
    switch (state) {
        case S1:
            if ((rf_info.has_req && rf_info.rf_hit) || step_info.is_forward) {
                state = S2;
            }
            break;
        case S2:
            S2_check_and_return();
            if (data_back_info.fire) {
                state = S1;
            }
            break;
        default:
            assert(false);
            break;
    }

    reset_info();
}

bool UnreadyQueue::is_matching_entry(const URQEntry& entry, uint64_t index, uint64_t bank) const {
    auto [entry_tag, entry_index, entry_bank] = cache->get_tag_index_bank(entry.t);
    // No need to check tag here, as we assume all entries in URQ are from "hit" unready
    return entry.valid && entry_index == index && entry_bank == bank;
}