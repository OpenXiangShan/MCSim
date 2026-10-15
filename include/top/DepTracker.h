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

#ifndef DEPTRACKER_H
#define DEPTRACKER_H

#include <array>
#include "utils/common.h"
#include "top/Transaction.h"

class DepTracker {
private:
    static constexpr int MAX_DEPS = 64;

    struct Record {
        bool valid = false;
        bool sent = false;
        TIME_TYPE expect_send_cycle = 0;
        TIME_TYPE send_cycle = 0;
        bool returned = false;
        TIME_TYPE expect_return_cycle = 0;
        TIME_TYPE return_cycle = 0;
        SysTransaction st{};
        int channel = 0;
    };

    std::array<Record, MAX_DEPS> records{};
    int next_pos = 0;

    uint64_t logical_index(uint64_t pos) const;
    bool has_dependency_on(uint64_t record_pos, uint64_t dependency_pos) const;
    bool dependency_ready(uint64_t pos) const;
    bool can_send(uint64_t pos, TIME_TYPE current_cycle) const;
    bool same_trace(uint64_t pos, uint64_t id, bool is_read, int channel) const;
    void propagate_return_effects(uint64_t returned_pos);

public:
    DepTracker() = default;
    bool add_record(SysTransaction st);
    bool get_trace(SysTransaction &st, TIME_TYPE current_cycle, int channel, int &record_pos);
    void mark_sent(int record_pos, TIME_TYPE cycle);
    void mark_returned(uint64_t id, bool read, int channel, TIME_TYPE cycle);
    bool full() const;
    bool all_done() const;
};

#endif   // DEPTRACKER_H
