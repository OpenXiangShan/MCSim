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

#include "top/DepTracker.h"
#include "AddrMap/AddrMap.h"
#include "config.h"
#include <cassert>

uint64_t DepTracker::logical_index(uint64_t pos) const
{
    return (pos - next_pos + MAX_DEPS) % MAX_DEPS;
}

bool DepTracker::has_dependency_on(uint64_t record_pos, uint64_t dependency_pos) const
{
    assert(record_pos < MAX_DEPS && dependency_pos < MAX_DEPS);

    const Record &record = records[record_pos];
    const Record &dependency = records[dependency_pos];
    if (!record.valid || !dependency.valid) {
        return false;
    }

    // Dependency must be an earlier logical record.
    if (logical_index(dependency_pos) >= logical_index(record_pos)) {
        return false;
    }

    const uint64_t dep = record.st.dependency;
    return dep != 0 && ((record_pos + MAX_DEPS - dependency_pos) % MAX_DEPS == dep);
}

bool DepTracker::dependency_ready(uint64_t pos) const
{
    assert(pos < MAX_DEPS);

    const Record &record = records[pos];
    if (!record.valid) {
        return false;
    }

    const uint64_t dep = record.st.dependency;
    if (dep == 0) {
        return true;
    }

    const uint64_t logic_pos = logical_index(pos);
    if (logic_pos < dep) {
        // The dependency is outside the current sliding window.
        // This may happen when trace is cut from another trace from the middle.
        // In such cases, the dependency cannot be waited on, so treat it as ready.
        return true;
    }

    const uint64_t dependency_pos = (pos + MAX_DEPS - dep) % MAX_DEPS;
    if (!records[dependency_pos].valid) {
        return true;
    }

    return has_dependency_on(pos, dependency_pos)
        && records[dependency_pos].sent
        && records[dependency_pos].returned;
}

bool DepTracker::can_send(uint64_t pos, TIME_TYPE current_cycle) const
{
    return current_cycle >= records[pos].expect_send_cycle && dependency_ready(pos);
}

bool DepTracker::same_trace(uint64_t pos, uint64_t id, bool is_read, int channel) const
{
    const Record &record = records[pos];
    return record.valid
        && record.st.id == id
        && record.st.is_read == is_read
        && record.channel == channel;
}

void DepTracker::propagate_return_effects(uint64_t returned_pos)
{
    Record &returned_record = records[returned_pos];

    assert(returned_pos < MAX_DEPS);
    assert(returned_record.valid && returned_record.sent && returned_record.returned);

    const int cycles_return_late = static_cast<int>(returned_record.return_cycle - returned_record.expect_return_cycle);

    for (int i = 1; i < MAX_DEPS; ++i) {
        const uint64_t candidate_pos = (returned_pos + i) % MAX_DEPS;
        Record &candidate = records[candidate_pos];

        // Stop once we wrap around to the logical front of the window.
        if (logical_index(candidate_pos) == 0) {
            break;
        }

        if (!candidate.valid) {
            continue;
        }

        if (!has_dependency_on(candidate_pos, returned_pos)) {
            continue;
        }

        assert(!candidate.sent);

        candidate.expect_send_cycle = returned_record.send_cycle
            + (candidate.st.timestamp - returned_record.st.timestamp) * timestamp_x
            + cycles_return_late;

        if (static_cast<int64_t>(candidate.expect_send_cycle) < 0) {
            candidate.expect_send_cycle = 0;
        }
    }
}

bool DepTracker::add_record(SysTransaction st)
{
    if (full()) {
        return false;
    }

    Record &record = records[next_pos];
    record = Record{};
    record.valid = true;
    record.expect_send_cycle = st.timestamp * timestamp_x;
    record.st = st;
    record.channel = calculate_channel(st.addr);

    const uint64_t record_pos = next_pos;
    next_pos = (next_pos + 1) % MAX_DEPS;

    // If the dependency entry already returned, update the new record's send cycle now.
    if (st.dependency != 0) {
        const uint64_t dependency_pos = (record_pos + MAX_DEPS - st.dependency) % MAX_DEPS;
        if (records[dependency_pos].valid
            && records[dependency_pos].returned
            && has_dependency_on(record_pos, dependency_pos)) {
            propagate_return_effects(dependency_pos);
        }
    }

    return true;
}

bool DepTracker::get_trace(SysTransaction &st, TIME_TYPE current_cycle, int channel, int &record_pos)
{
    for (int i = 0; i < MAX_DEPS; ++i) {
        const int pos = (next_pos + i) % MAX_DEPS;
        const Record &record = records[pos];

        if (!record.valid || record.sent) {
            continue;
        }
        if (channel >= 0 && record.channel != channel) {
            continue;
        }
        if (!can_send(pos, current_cycle)) {
            continue;
        }

        // return one trace that can be sent on the specified channel
        st = record.st;
        record_pos = pos;
        return true;
    }

    record_pos = -1;
    return false;
}

void DepTracker::mark_sent(int record_pos, TIME_TYPE cycle)
{
    assert(record_pos >= 0 && record_pos < MAX_DEPS);
    assert(records[record_pos].valid && !records[record_pos].sent);

    Record &record = records[record_pos];
    record.sent = true;
    record.send_cycle = cycle;
    record.expect_return_cycle = cycle + average_axi_latency;
}

void DepTracker::mark_returned(uint64_t id, bool is_read, int channel, TIME_TYPE cycle)
{
    // Search in logical order and match the oldest outstanding record.
    for (int i = 0; i < MAX_DEPS; ++i) {
        const int pos = (next_pos + i) % MAX_DEPS;
        Record &record = records[pos];

        if (!same_trace(pos, id, is_read, channel) || !record.sent || record.returned) {
            continue;
        }

        record.returned = true;
        record.return_cycle = cycle;
        propagate_return_effects(pos);
        return;
    }

    assert(false && "failed to match returned trace");
}

bool DepTracker::full() const
{
    return records[next_pos].valid && !records[next_pos].returned;
}

bool DepTracker::all_done() const
{
    for (const auto &record : records) {
        if (record.valid && !record.returned) {
            return false;
        }
    }
    return true;
}
