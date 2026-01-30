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

#ifndef MSHR_H
#define MSHR_H
#include "top/Transaction.h"
#include "Cache/Cache.h"
#include <assert.h>
#include <unordered_map>
#include <list>
#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>

struct MSHREntry {
    MappedTransaction t;
    enum class State {
        AWAITING_ISSUE,   // wait for request to be ISSUED
        REQUEST_ISSUED   // issued but not completed
    } state;
    std::list<MappedTransaction> merged_requests;   // requests with the same addr

    MSHREntry(MappedTransaction t) : state(State::AWAITING_ISSUE), t(t), merged_requests() {}
};

class MSHR {
private:
    // hash from addr to MSHREntry list iter
    std::unordered_map<uint64_t, std::list<MSHREntry>::iterator> mshr_map;
    std::list<MSHREntry> mshr_list;
    size_t max_entries;
    bool add_mshr_entry(MSHREntry mshr_entry);
    bool remove_mshr_entry(MSHREntry mshr_entry);
public:
    MSHR(size_t capacity) : max_entries(capacity) {}
    bool full() {
        return mshr_list.size() >= max_entries;
    }
    bool empty() {
        return mshr_list.empty();
    }
    std::list<MSHREntry>::iterator get_mshr_entry(MappedTransaction t);
    std::pair<MappedTransaction, bool> get_unissued_request();
    bool add_request(MappedTransaction t);
    void remove_request(MappedTransaction t);   // undo add request
    void issue_request(MappedTransaction t);   // t has been issued
    void finish_request(MappedTransaction t);   // t has been completed
};

#endif   // MSHR_H