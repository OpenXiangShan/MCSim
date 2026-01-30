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

#ifndef __DRIVER_H__
#define __DRIVER_H__
#include <cstdint>
#include <random>
#include <fstream>
#include <set>
#include "top/Transaction.h"
class IdAllocator {
    // An ID is invalid if it doesn't return from MC;
    // For all valid IDs, allocate the least one.
    public:
        IdAllocator() : max_id(2<<14-1), next_rid(0), next_wid(0) {}
        ID_TYPE allocate_rid();
        ID_TYPE allocate_wid();
        void release_rid(ID_TYPE rid);
        void release_wid(ID_TYPE wid);
        ID_TYPE next_rid;
        ID_TYPE next_wid;
        ID_TYPE max_id;
    private:
        std::set<ID_TYPE> free_rids;
        std::set<ID_TYPE> free_wids;
};

class Driver {
    public:
        virtual bool next_transaction() = 0;
        SysTransaction& get_transaction() {return st;}
        IdAllocator *ia;
    protected:
        SysTransaction st;
};

class RandomDriver: public Driver {
    public:
        RandomDriver(double read_ratio, int limit): read_ratio(read_ratio * 0x100000), limit(limit), generated(0) {st.timestamp = 0;};
        RandomDriver(): read_ratio(0.5 * 0x100000) {};
        bool next_transaction() override;
    private:
        int read_ratio;
        int limit;
        int generated;
};

class TraceDriver: public Driver {
    public:
        TraceDriver(const char* filename);
        bool next_transaction() override;
    private:
        std::ifstream trace;
};
#endif // __DRIVER_H__