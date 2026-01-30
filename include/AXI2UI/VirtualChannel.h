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

#ifndef VIRTUALCHANNEL_H
#define VIRTUALCHANNEL_H
#include "utils/FixedQueue.h"
#include "utils/common.h"
#include <map>
#include "config.h"
#include "top/Transaction.h"
class IdAllocator;
class Log;
class VirtualChannel {
    public:
        size_t vc_id;
        std::vector<TIME_TYPE> exp_rdback_time;
        std::vector<TIME_TYPE> acc_time;
        std::map<uint32_t, uint32_t> rd_latency;
        FixedQueue<std::pair<ID_TYPE, TOKEN_TYPE>> rdback_buffer;
        FixedQueue<TOKEN_TYPE> available_tokens;
        VirtualChannel(size_t vc_id, size_t tok_cap);
};
class StaticVirtualChannel {
    public:
        std::array<VirtualChannel*, Axi2uiConfig::vc_num> vcs;
        struct {
            public:
                uint64_t rob_full;
                uint64_t total_latency, read_count;
        } stats;
        StaticVirtualChannel(size_t tok_cap);
        void put_back(ID_TYPE id, TOKEN_TYPE token, uint64_t exp_rdback_time);
        bool addTrans(AxiTransaction &at);
        void step(uint64_t cycles, uint64_t &next_valid_rd_time, IdAllocator *ia, Log *log);
        bool all_done() const;
    private:
        size_t vc_id(ID_TYPE id);   // hash from id to vc_id
};
class DynamicVirtualChannel {
    public:
        struct {
            public:
                uint64_t rob_full, max_rob_size;
                uint64_t total_latency, read_count;
        } stats;
        void put_back(ID_TYPE id, TOKEN_TYPE token, uint64_t exp_rdback_time);
        bool addTrans(AxiTransaction &at);
        void step(uint64_t cycles, uint64_t &next_valid_rd_time, IdAllocator *ia, Log *log);
        bool all_done() const;
        DynamicVirtualChannel();
    private:
        static constexpr ID_TYPE INVALID_ID = ID_TYPE(-1);
        static constexpr TOKEN_TYPE INVALID_TOKEN = TOKEN_TYPE(-1);
        static constexpr size_t capacity = Axi2uiConfig::vc_num * Axi2uiConfig::rob_size;
        size_t size;
        // whether data return from dfi
        std::array<bool, capacity> valid_bitmap;
        // whether entry is available
        std::array<bool, capacity> free_bitmap;
        // whether entry is a specific id head
        std::array<bool, capacity> head_bitmap;
        // map from id to offset(i.e., token)
        std::map<ID_TYPE, TOKEN_TYPE> id_tail;
        // stores id of this entry and offset of next entry; (-1) if no next entry
        std::array<std::pair<ID_TYPE, TOKEN_TYPE>, capacity> rdback_buffer;
        std::vector<TIME_TYPE> exp_rdback_time;
        std::vector<TIME_TYPE> acc_time;
        std::map<uint32_t, uint32_t> rd_latency;
        bool full() const;
        bool empty() const;
        void data_in(ID_TYPE id, TOKEN_TYPE offset);
        bool data_out(ID_TYPE &id, TOKEN_TYPE &offset);
        TOKEN_TYPE cmd_in(ID_TYPE id);
        TOKEN_TYPE get_one_valid_head();
        TOKEN_TYPE get_one_free();
};

#endif // VIRTUALCHANNEL_H