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

#include "AXI2UI/VirtualChannel.h"
#include "AXI2UI/AXI2UI.h"
#include "utils/debug.h"
#include "utils/log.h"
#include "top/Driver.h"

DynamicVirtualChannel::DynamicVirtualChannel()
    : size(0),
      exp_rdback_time(capacity, -1),
      acc_time(capacity, 0)
{
    for (auto &entry : valid_bitmap) {
        entry = false;
    }
    for (auto &entry: free_bitmap) {
        entry = true;
    }
    for (auto &entry: head_bitmap) {
        entry = false;
    }

    for (auto &entry : rdback_buffer) {
        entry = std::make_pair(INVALID_ID, INVALID_TOKEN);
    }

    stats.rob_full = 0;
    stats.max_rob_size = 0;
    stats.total_latency = 0;
    stats.read_count = 0;
}

bool DynamicVirtualChannel::full() const
{
    assert(size <= capacity);
    return size == capacity;
}

bool DynamicVirtualChannel::empty() const
{
    return size == 0;
}

void DynamicVirtualChannel::data_in(ID_TYPE id, TOKEN_TYPE offset)
{
    assert(id == rdback_buffer[offset].first);
    // set valid_bitmap
    valid_bitmap[offset] = true;
}

bool DynamicVirtualChannel::data_out(ID_TYPE &ret_id, TOKEN_TYPE &ret_offset)
{
    // get one head for get data out of AXI
    TOKEN_TYPE offset = get_one_valid_head();
    if (offset == INVALID_TOKEN) {
        return false;
    }
    assert(head_bitmap[offset] && valid_bitmap[offset] && !free_bitmap[offset]);
    // set head.next
    ID_TYPE this_id = rdback_buffer[offset].first;
    TOKEN_TYPE next_head = rdback_buffer[offset].second;
    if (next_head != INVALID_TOKEN) {
        assert(!head_bitmap[next_head]);
        head_bitmap[next_head] = true;
    } else {
        // is tail
        assert(id_tail[this_id] == offset);
        id_tail[this_id] = INVALID_TOKEN;
    }
    // release head
    head_bitmap[offset] = false;
    valid_bitmap[offset] = false;
    free_bitmap[offset] = true;

    assert(size-- > 0);
    ret_id = this_id;
    ret_offset = offset;
    return true;
}

TOKEN_TYPE DynamicVirtualChannel::cmd_in(ID_TYPE id)
{
    // check free_bitmap and return one free entry offset;
    TOKEN_TYPE free_offset = get_one_free();
    // return (-1) if no free entry;
    if (free_offset == INVALID_TOKEN) {
        return INVALID_TOKEN;
    }
    assert(free_bitmap[free_offset]);
    free_bitmap[free_offset] = false;
    // set head_bitmap and id_tail
    bool is_head = true;
    if (id_tail.contains(id)) {
        is_head = (id_tail[id] == INVALID_TOKEN);
    }
    if (is_head) {
        assert(!head_bitmap[free_offset]);
        head_bitmap[free_offset] = true;
    } else {
        assert(rdback_buffer[id_tail[id]].second == INVALID_TOKEN);
        rdback_buffer[id_tail[id]].second = free_offset;
    }
    id_tail[id] = free_offset;
    // set rdback_buffer
    rdback_buffer[free_offset] = std::make_pair(id, INVALID_TOKEN);

    assert(++size <= capacity);
    stats.max_rob_size = std::max(stats.max_rob_size, size);
    return free_offset;
}

TOKEN_TYPE DynamicVirtualChannel::get_one_valid_head()
{
    // RoundRobin
    static TOKEN_TYPE arb_pointer = 0;
    for (TOKEN_TYPE i = 1; i <= capacity; i++) {
        TOKEN_TYPE offset = (arb_pointer + i) % capacity;
        if (head_bitmap[offset] && valid_bitmap[offset]) {
            return offset;
        }
    }
    return INVALID_TOKEN;
}

TOKEN_TYPE DynamicVirtualChannel::get_one_free()
{
    for (TOKEN_TYPE i = 0; i < capacity; i++) {
        if (free_bitmap[i]) {
            return i;
        }
    }
    return INVALID_TOKEN;
}

void DynamicVirtualChannel::put_back(ID_TYPE id, TOKEN_TYPE token, uint64_t exp_rdback_time)
{
    assert(token < this->exp_rdback_time.size());
    assert(this->exp_rdback_time[token] == -1);
    this->exp_rdback_time[token] = exp_rdback_time;
}

bool DynamicVirtualChannel::addTrans(AxiTransaction &at)
{
    if (at.is_read) {
        TOKEN_TYPE token = cmd_in(at.id);
        if (token == INVALID_TOKEN) {
            stats.rob_full++;
            return false;
        }
        assert(token < capacity);
        exp_rdback_time[token] = -1;
        acc_time[token] = at.cycle;

        at.token = token;
    }
    return true;
}

void DynamicVirtualChannel::step(uint64_t cycles, uint64_t &next_valid_rd_time, IdAllocator *ia, Log *log)
{
    // data out axi
    if (next_valid_rd_time <= cycles) {
        ID_TYPE id;
        TOKEN_TYPE token;
        if (data_out(id, token)) {
            ia->release_rid(id);
            log->out_axi(id, token);
            next_valid_rd_time = cycles + interval;
        }
    }

    // check exp_rdback_time, data in rob
    static TOKEN_TYPE arb_pointer = 0;
    for (TOKEN_TYPE i = 1; i <= capacity; i++) {
        TOKEN_TYPE offset = (arb_pointer + i) % capacity;
        if (exp_rdback_time[offset] <= cycles) {
            uint64_t latency = cycles - acc_time[offset];
            rd_latency[latency]++;
            stats.total_latency += latency;
            stats.read_count++;
            // dump latency
            if (DumpConfig::DUMP_LATENCY) {
                std::cout /*<< "returned token: " << token << ", latency: "*/<< latency << std::endl;
            }
            data_in(rdback_buffer[offset].first, offset);
            exp_rdback_time[offset] = -1;
        }
    }
}

bool DynamicVirtualChannel::all_done() const
{
    return empty();
}