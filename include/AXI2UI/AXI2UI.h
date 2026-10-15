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

#ifndef AXI2UI_H
#define AXI2UI_H
#include "utils/FixedQueue.h"
#include "utils/common.h"
#include "top/Transaction.h"
#include <cstdint>
#include <vector>
#include <iostream>
#include <cassert>
#include <map>
#include "config.h"

class Filter;
class Log;
class IdAllocator;
class StaticVirtualChannel;
class DynamicVirtualChannel;
class AXI2UI {
    private:
        const uint64_t &cycles;
        int channel_id;
        uint64_t next_valid_rd_time;   // back to axi master
        StaticVirtualChannel* svc;
        DynamicVirtualChannel* dvc;
        FixedQueue<SysTransaction> arInTrans;
        FixedQueue<std::pair<AxiTransaction, uint64_t>> arOutTrans;   // <at, cycle>
        FixedQueue<SysTransaction> awInTrans;
        FixedQueue<std::pair<AxiTransaction, uint64_t>> awOutTrans;   // <at, cycle>
        FixedQueue<ID_TYPE> BChannelTrans;
        static constexpr uint64_t UI_TRANSACTION_BYTES = 64;
        static bool rangesOverlap(uint64_t lhs_addr, uint64_t rhs_addr);
        bool hasPendingReadConflict(uint64_t addr) const;
        bool hasPendingWriteConflict(uint64_t addr) const;
        bool prepareReadForFilter(AxiTransaction &at);
        bool commitReadToVc(AxiTransaction &at);
        // statistics
        struct statistics_data {
            public:
                uint64_t rtrans_full;
                uint64_t wtrans_full;
                uint64_t read_count;      // Total read requests received
                uint64_t write_count;     // Total write requests received
        } stats;

    public:
        AXI2UI(const uint64_t &cycles, size_t tok_cap, int channel_id = 0);
        void burst_clip();
        void step();
        void put_back(ID_TYPE id, TOKEN_TYPE token, CallerType caller);
        void put_back_at(ID_TYPE id, TOKEN_TYPE token, TIME_TYPE exp_rdback_time, CallerType caller);
        void put_back_ready(ID_TYPE id, TOKEN_TYPE token, CallerType caller);
        bool addTrans(SysTransaction);
        bool willAccept(uint64_t addr, bool is_read) const;
        bool all_done() const;
        void statistics() const;
        void resetStats();
        void print_latency_distribution() const;
        Filter *ft;
        Log *log;
        IdAllocator *ia;
};
#endif // AXI2UI_H
