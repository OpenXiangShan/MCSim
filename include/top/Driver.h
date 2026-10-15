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
class Driver;
class DepTracker;
class Controller;
class IdAllocator {
    // An ID is invalid if it doesn't return from MC;
    // For all valid IDs, allocate the least one.
    public:
        IdAllocator(Driver *d)
            // 中文说明：
            // 1) max_id 原表达式 2<<14-1 在 -Werror 下会触发优先级告警；
            // 2) 按成员声明顺序初始化，避免 -Wreorder。
            : next_rid(0), next_wid(0), max_id((1u << 14) - 1),
              driver(d), controller(nullptr) {}

        // 中文说明：
        // 为了把 mcsim 嵌入到 gem5 中，我们需要在“读/写完成”时把事件
        // 传回 gem5（类似 DRAMsim3 的 read/write callback）。
        // 这两个回调是可选的：mcsim 单独运行时可不设置。
        std::function<void(ID_TYPE)> on_read_complete;
        std::function<void(ID_TYPE)> on_write_complete;

        ID_TYPE allocate_rid();
        ID_TYPE allocate_wid();
        void release_rid(ID_TYPE rid, int channel = 0);
        void release_wid(ID_TYPE wid, int channel = 0);
        ID_TYPE next_rid;
        ID_TYPE next_wid;
        ID_TYPE max_id;
        Driver *driver;
        Controller *controller;
    private:
        std::set<ID_TYPE> free_rids;
        std::set<ID_TYPE> free_wids;
};

class Driver {
    public:
        virtual bool next_transaction() = 0;
        SysTransaction& get_transaction() {return st;}
        IdAllocator *ia;
        DepTracker *dep_tracker;
        struct {
            ID_TYPE max_rid = 0;
        } stats;   // statistics
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