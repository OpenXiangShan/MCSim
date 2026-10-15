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

#include "top/Driver.h"
#include "top/DepTracker.h"
#include "top/Controller.h"
#include "config.h"
#include <iostream>
#include <cassert>
bool RandomDriver::next_transaction()
{
    if (generated >= limit)
        return false;
    st.timestamp = 0;
    st.is_read = (rng() & 0xfffff)  < read_ratio;
    st.addr = rng() & 0x3ffffffff;   // 34 bits
    st.id = st.is_read ? ia->allocate_rid() : ia->allocate_wid();
    stats.max_rid = st.is_read ? std::max(stats.max_rid, st.id) : stats.max_rid;
    if (NEW_TRACE) {
        st.dependency = rng() % 64;
    }
    ++generated;
    return true;
}

TraceDriver::TraceDriver(const char* filename)
{
    // const size_t buff_size = 32 * 1024 * 1024;
    // char *buff = new char[buff_size];
    trace.open(filename, std::ios::in);
    // if (!trace.is_open() || !buff) {
    if (!trace.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        exit(1);
    }
    // trace.rdbuf()->pubsetbuf(buff, buff_size);
}

bool TraceDriver::next_transaction()
{
    if (trace.eof())
        return false;
    trace >> st;
    if (trace.fail()) {
        return false;
    }
    if (!NEW_TRACE) {
        st.id = st.is_read ? ia->allocate_rid() : ia->allocate_wid();
    }
    stats.max_rid = st.is_read ? std::max(stats.max_rid, st.id) : stats.max_rid;
    return true;
}

ID_TYPE IdAllocator::allocate_rid()
{
    // check free_rids
    if (!free_rids.empty()) {
        ID_TYPE rid = *free_rids.begin();
        free_rids.erase(free_rids.begin());
        return rid;
    }
    // no free_rids, allocate new id
    if (next_rid == max_id) {
        // TODO: we expect this never happens,
        //       but if happen, we should stop sending trace
        std::cerr << "Out of read ids" << std::endl;
        exit(1);
    }
    return next_rid++;
}

ID_TYPE IdAllocator::allocate_wid()
{
    // check free_wids
    if (!free_wids.empty()) {
        ID_TYPE wid = *free_wids.begin();
        free_wids.erase(free_wids.begin());
        return wid;
    }
    // no free_wids, allocate new id
    if (next_wid == max_id) {
        // TODO: we expect this never happens,
        //       but if happen, we should stop sending trace
        std::cerr << "Out of write ids" << std::endl;
        exit(1);
    }
    return next_wid++;
}

void IdAllocator::release_rid(ID_TYPE rid, int channel)
{
    // 中文说明：先通知外部（例如 gem5）该读请求已完成。
    // 注意：这个回调不应改变 mcsim 的内部资源回收逻辑。
    if (on_read_complete) {
        on_read_complete(rid);
    }

    if (NEW_TRACE) {
        // 中文说明：driver 可能为空（gem5 嵌入模式下不使用 Driver/DepTracker）。
        if (driver && driver->dep_tracker && controller) {
            driver->dep_tracker->mark_returned(rid, true, channel, controller->get_cycles());
        }
    }

    // if NEW_TRACE and driver is TraceDriver, don't need to release
    if (NEW_TRACE && driver && dynamic_cast<TraceDriver*>(driver)) {
        return;
    }

    assert(rid < next_rid);
    assert(free_rids.find(rid) == free_rids.end());
    free_rids.insert(rid);
}

void IdAllocator::release_wid(ID_TYPE wid, int channel)
{
    // 中文说明：先通知外部（例如 gem5）该写请求已完成。
    if (on_write_complete) {
        on_write_complete(wid);
    }

    if (NEW_TRACE) {
        if (driver && driver->dep_tracker && controller) {
            driver->dep_tracker->mark_returned(wid, false, channel, controller->get_cycles());
        }
    }

    // if NEW_TRACE and driver is TraceDriver, don't need to release
    if (NEW_TRACE && driver && dynamic_cast<TraceDriver*>(driver)) {
        return;
    }

    assert(wid < next_wid);
    assert(free_wids.find(wid) == free_wids.end());
    free_wids.insert(wid);
}