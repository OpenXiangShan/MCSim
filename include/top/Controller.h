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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "utils/FixedQueue.h"
#include "config.h"
#include "AXI2UI/AXI2UI.h"
#include "Filter/Filter.h"
#include "AddrMap/AddrMap.h"
#include "Cache/Cache.h"
#include "Scg/SdramCommandGen.h"
#include "Scheduler/TransactionSchedule.h"
#include "utils/log.h"
#include <array>
#include <string>

class AXI2UI;
class Filter;
class AddrMap;
class Cache;
class SdramCommandGen;
class TransactionSchedule;
class Log;
class IdAllocator;

// Per-channel controller components
struct ChannelComponents {
    AXI2UI* axi2ui;
    Filter* ft;
    AddrMap* am;
    Cache* cache;
    SdramCommandGen* scg;
    TransactionSchedule* scheduler;
    
    ChannelComponents() : axi2ui(nullptr), ft(nullptr), am(nullptr), cache(nullptr), 
                          scg(nullptr), scheduler(nullptr) {}
};

class Controller {
    private:
        std::array<ChannelComponents, NUM_CHANNELS> channels;
        std::array<int, NUM_CHANNELS> readback_rr_next;
        Log log;
        TIME_TYPE cycles;
        void arbitrate_readback(int ch);
        
    public:
#ifdef GEM5_MODE
        Controller(IdAllocator *ia);
#else
        Controller(IdAllocator *ia, const std::string &log_dir = DumpConfig::dump_log_dir_path);
#endif
        ~Controller();
        void step();
        bool all_done() const;
        void statistics() const;
        void resetStats();
        bool addTrans(const SysTransaction& st);
        // 中文说明：仅查询是否能接收事务（无副作用）。
        bool willAccept(uint64_t addr, bool is_write) const;
        TIME_TYPE get_cycles() const;
};

#endif // CONTROLLER_H