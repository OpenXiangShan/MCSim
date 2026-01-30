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
        uint64_t cycles;
        uint64_t next_valid_rd_time, next_valid_wr_time;   // back to axi master
        StaticVirtualChannel* svc;
        DynamicVirtualChannel* dvc;
        FixedQueue<SysTransaction> arInTrans;
        FixedQueue<std::pair<AxiTransaction, uint64_t>> arOutTrans;   // <at, cycle>
        FixedQueue<SysTransaction> awInTrans;
        FixedQueue<std::pair<AxiTransaction, uint64_t>> awOutTrans;   // <at, cycle>
        // statistics
        struct statistics_data {
            public:
                uint64_t rtrans_full;
                uint64_t wtrans_full;
        } stats;

    public:
        AXI2UI(size_t tok_cap);
        void burst_clip();
        void step();
        TIME_TYPE get_cycles() const;
        void put_back(ID_TYPE id, TOKEN_TYPE token, CallerType caller);
        bool addTrans(SysTransaction&);
        bool all_done() const;
        void statistics() const;
        Filter *ft;
        Log *log;
        IdAllocator *ia;
};
#endif // AXI2UI_H