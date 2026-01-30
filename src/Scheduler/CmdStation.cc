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

#include "Scheduler/CmdStation.h"



CmdStation::CmdStation(int cap, int l_water, int h_water): size(0), grant(false), capacity(cap),
    l_water(l_water), h_water(h_water), conflict_count(0), conflict(false), conflict_slot(nullptr)
{
    switch (station_org) {
        case StationOrg::SINGLE:
            queue_num = all_bank_num;
            break;
        case StationOrg::PER_BANK:
            queue_num = 1;
            break;
        case StationOrg::PER_BG:
            queue_num = bank_per_bg;
            break;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            break;
    }

    valid_list.resize(queue_num);
    valid_tail.resize(queue_num, nullptr);
    open_rows.resize(queue_num);
    bank_conflict.resize(queue_num, false);
    // capacity = 

    free_list.next = nullptr;
    auto nodes = new cmd_slot[capacity];
    for (int i = 0; i < capacity; ++i) {
        nodes[i].next = free_list.next;
        free_list.next = &nodes[i];
    }

    for (int i = 0; i < queue_num; ++i) {
        valid_list[i].next = nullptr;
        valid_tail[i] = nullptr;
        valid_list[i].next = valid_tail[i];
    }
}

bool CmdStation::almost_full() const
{
    assert(h_water <= capacity);
    return size >= h_water;
}

bool CmdStation::almost_empty() const
{
    assert(l_water >= 0);
    return size <= l_water;
}

void CmdStation::set_grant(bool g)
{
    grant = g;
}


bool CmdStation::empty() const
{
    return size == 0;
}

size_t CmdStation::get_num(size_t bank_id)
{
    int queue_idx;
    switch (station_org) {
        case StationOrg::SINGLE:
            queue_idx = bank_id;
            break;
        case StationOrg::PER_BANK:
            queue_idx = 0;
            break;
        case StationOrg::PER_BG:
            queue_idx = bank_id % bank_per_bg;
            break;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            break;
    }
    size_t num = 0;
    for (auto item = valid_list[queue_idx].next; item != nullptr; item = item->next) {
        num++;
    }
    return num;


    // return size;
}

static inline int get_queue_idx(const MappedTransaction &trans)
{
    switch (station_org) {
        case StationOrg::SINGLE:
            return  trans.mapped_addr.bg * bank_per_bg + trans.mapped_addr.bank;
            break;
        case StationOrg::PER_BANK:
            return 0;
        case StationOrg::PER_BG:
            return  trans.mapped_addr.bank;
            break;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            return 0;
            break;
    }

}

bool CmdStation::check_conflict(const MappedTransaction &trans)
{
    // int bank_no = trans.mapped_addr.bg * bank_per_bg + trans.mapped_addr.bank;
    int queue_idx = get_queue_idx(trans);

    // return false; // ignore conflict
    if (conflict) return true;
    for (auto item = valid_list[queue_idx].next; item != nullptr; item = item->next) {
        if (item->ts.addr == trans.addr) {
            conflict_slot = item;
            conflict_count++;
            bank_conflict[queue_idx] = true;
            conflict = true;
            return true;
        }
    }
    return false;
}

bool CmdStation::addTrans(const MappedTransaction &trans)
{
    int queue_idx = get_queue_idx(trans);
    if (conflict) {
        return false;
    }

    if (size == capacity) { // full
        return false;
    }

    cmd_slot *empty_slot = free_list.next;
    free_list.next = empty_slot->next;
    empty_slot->ts = trans;

    if (valid_tail[queue_idx] == nullptr) {
        valid_list[queue_idx].next = empty_slot;
        empty_slot->prev = &valid_list[queue_idx];
    } else {
        valid_tail[queue_idx]->next = empty_slot;
        empty_slot->prev = valid_tail[queue_idx];
    }
    valid_tail[queue_idx] = empty_slot;
    empty_slot->next = nullptr;
    ++size;

    return true;
}

bool CmdStation::schedule_one(int queue_idx)
{
    if (valid_list[queue_idx].next == nullptr) {
        return false;
    }

    if (bank_conflict[queue_idx]) {
        schedule_slot = conflict_slot;
        return true;
    }

// fr-fcfs
    if (schedule_policy == SchedulePolicy::FRFCFS)
        for (auto item = valid_list[queue_idx].next; item != nullptr; item = item->next) {
            if (open_rows[queue_idx] == item->ts.mapped_addr.row) {
                schedule_slot = item;
                return true;
            }
        }
// fcfs
    schedule_slot = valid_list[queue_idx].next;
    return true;
}

void CmdStation::step()
{
    // each bank can issue one command per cycle
    for (int i = 0; i < queue_num; ++i) {
        if (grant && bank_free[i] && schedule_one(i)) {
            bank_free[i] = false;
            assert(scg->addTrans(schedule_slot->ts));
            --size;
            open_rows[i] = schedule_slot->ts.mapped_addr.row;
            if (conflict_slot == schedule_slot) {
                bank_conflict[i] = false;
                conflict = false;
            }
            schedule_slot->prev->next = schedule_slot->next; 
            if (schedule_slot == valid_tail[i]) {
                assert(schedule_slot->next == nullptr);
                valid_tail[i] = schedule_slot->prev;
                if (valid_tail[i] == &valid_list[i]) {
                    valid_tail[i] = nullptr;
                }
            } else {
                schedule_slot->next->prev = schedule_slot->prev;
            }
            schedule_slot->next = free_list.next;
            free_list.next = schedule_slot;
        }
    }
}