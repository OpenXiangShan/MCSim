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

class AXI2UI;
class Filter;
class AddrMap;
class Cache;
class SdramCommandGen;
class TransactionSchedule;
class Log;
class IdAllocator;

class Controller {
    private:
        AXI2UI axi2ui;
        Filter ft;
        AddrMap am;
        Cache cache;
        SdramCommandGen scg;
        TransactionSchedule scheduler;
        Log log;
        TIME_TYPE cycles;
    public:
        Controller(IdAllocator *ia);
        void step();
        bool all_done() const;
        void statistics() const;
        bool addTrans(SysTransaction& st);
        TIME_TYPE get_cycles() const;
};

#endif // CONTROLLER_H