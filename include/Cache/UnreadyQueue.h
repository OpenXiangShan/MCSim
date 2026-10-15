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

#ifndef UNREADYQUEUE_H
#define UNREADYQUEUE_H
#include "utils/common.h"
#include "top/Transaction.h"
#include "config.h"
#include <array>

class Cache;

class URQEntry {
    public:
        bool valid = false;
        MappedTransaction t;
};

class UnreadyQueue {
    public:
        static constexpr int EntryNum = 4;
        UnreadyQueue() { state = S1; };
        std::array<std::array<URQEntry, EntryNum>, CacheConfig::CACHE_WAYS> URQueue;
        enum State {
            S1,   // S1 receive the req from refillPipe
            S2   // S2 send data to AddrMap
        } state;
        Cache *cache;
        // req from mainPipe
        bool addTrans(MappedTransaction t, uint64_t way);
        // data from refillPipe
        bool returnDataFromRefillPipe(MappedTransaction t, uint64_t way);
        void step();
    private:
        struct {
            bool is_forward = false;
        } step_info;
        struct {
            bool has_req = false;
            bool fire = false;
            uint64_t way = -1;   // reasonless init
            MappedTransaction t;
        } mp_info;
        struct {
            bool has_req = false;
            bool fire = false;
            uint64_t way = -1;   // reasonless init
            MappedTransaction t;
            bool rf_hit = false;
        } rf_info;
        struct {
            bool fire = false;
        } data_back_info;
        void reset_info();
        void update_step_info();
        void S2_check_and_return();
        bool is_matching_entry(const URQEntry& entry, uint64_t index, uint64_t bank) const;
};

#endif   // UNREADYQUEUE_H