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

#include "Cache/MSHR.h"

bool MSHR::add_mshr_entry(MSHREntry mshr_entry)
{
    if (full()) {
        return false;
    }
    // auto it = mshr_list.emplace_back(mshr_entry);
    auto it = mshr_list.insert(mshr_list.end(), mshr_entry);
    mshr_map[mshr_entry.t.addr] = it;
    return true;
}

bool MSHR::remove_mshr_entry(MSHREntry mshr_entry)
{
    auto map_it = mshr_map.find(mshr_entry.t.addr);
    if (map_it == mshr_map.end()) {
        return false;
    }
    mshr_list.erase(map_it->second);
    mshr_map.erase(map_it);
    return true;
}

std::list<MSHREntry>::iterator MSHR::get_mshr_entry(MappedTransaction t)
{
    auto map_it = mshr_map.find(t.addr);
    assert(map_it != mshr_map.end());
    return map_it->second;
}

std::pair<MappedTransaction, bool> MSHR::get_unissued_request()
{
    if (empty()) {
        return std::make_pair(MappedTransaction(), false);
    }
    for (auto it = mshr_list.begin(); it != mshr_list.end(); ++it) {
        if (it->state != MSHREntry::State::AWAITING_ISSUE) {
            continue;
        }
        return std::make_pair(it->t, true);
    }
    return std::make_pair(MappedTransaction(), false);
}

bool MSHR::add_request(MappedTransaction t)
{
    auto map_it = mshr_map.find(t.addr);
    
    // MSHREntry with the same addr exists
    if (map_it != mshr_map.end()) {
        if (!CacheConfig::pfc_cfg.ENABLE) {
            assert(false);   // should have been blocked
        } else {
            return false;
            // TODO:
            // Prefetcher may request the same addr.
            // When it happens, just ignore the request.
            // return true;
        }
        // TODO: merge_requests set capacity (Baiyang doesn't use this method)
        // map_it->second->merged_requests.emplace_back(t);
        // return true;   // not new, success
    }
        
    // MSHREntry with the same addr doesn't exists, new entry needed
    if (add_mshr_entry(MSHREntry(t))) {
        return true;   // new, success
    }

    // MSHR full, add failed
    return false;   // new, not success
}

void MSHR::remove_request(MappedTransaction t)
{
    finish_request(t);
}

void MSHR::issue_request(MappedTransaction t)
{
    auto map_it = mshr_map.find(t.addr);
    assert(map_it != mshr_map.end());
    map_it->second->state = MSHREntry::State::REQUEST_ISSUED;
}

void MSHR::finish_request(MappedTransaction t)
{
    auto map_it = mshr_map.find(t.addr);
    assert(map_it != mshr_map.end());
    assert(remove_mshr_entry(map_it->second->t));
}