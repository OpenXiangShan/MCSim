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

VirtualChannel::VirtualChannel(size_t vc_id, size_t tok_cap)
    : vc_id(vc_id),
      exp_rdback_time(tok_cap, -1),
      acc_time(tok_cap, 0),
      rdback_buffer(tok_cap),
      available_tokens(tok_cap)
{
    for (int i = 0; i < tok_cap; ++i) {
        available_tokens.push(i);
    }
}

StaticVirtualChannel::StaticVirtualChannel(size_t tok_cap)
{
    size_t vc_id = 0;
    for (auto& vc : vcs) {
        vc = new VirtualChannel(vc_id, tok_cap);
        vc_id++;
    }
    stats.rob_full = 0;
    stats.total_latency = 0;
    stats.read_count = 0;
}

size_t StaticVirtualChannel::vc_id(ID_TYPE id)
{
    if (vcs.size() == 1) {
        return 0;
    } else {
        return id % vcs.size();
    }
}

void StaticVirtualChannel::put_back(ID_TYPE id, TOKEN_TYPE token, uint64_t exp_rdback_time)
{
    bool found_vc = false;
    bool found_vc_multiple = false;
    // get vc with the same id
    for (auto& vc : vcs) {
        if (vc->vc_id == vc_id(id)) {
            if (found_vc) {
                found_vc_multiple = true;
                break;
            }
            found_vc = true;
            // std::cout << "put back token: " << token << std::endl;
            debug("put back id %d\n, token %d\n", id, token);
            assert(token < vc->exp_rdback_time.size());
            assert(vc->exp_rdback_time[token] == -1);
            vc->exp_rdback_time[token] = exp_rdback_time;
        }
    }
    assert(found_vc && !found_vc_multiple);
}

bool StaticVirtualChannel::addTrans(AxiTransaction &at)
{
    if (at.is_read) {
        bool is_new = false;

        // get vc with the same vc_id
        auto vc_it = std::find_if(
            vcs.begin(), vcs.end(),
            [&at, this](const VirtualChannel* vc) {
                return vc->vc_id == this->vc_id(at.id);
            }
        );

        assert(vc_it != vcs.end());

        VirtualChannel *vc_p = *vc_it;
        assert(!is_new);
        if (is_new) {
            vc_p->vc_id = vc_id(at.id);
        }

        if (vc_p->available_tokens.empty() || vc_p->rdback_buffer.full()) {
            stats.rob_full++;
            return false;
        }
        TOKEN_TYPE token = vc_p->available_tokens.front();
        assert(token < vc_p->exp_rdback_time.size());
        vc_p->available_tokens.pop();
        vc_p->rdback_buffer.push(std::make_pair(at.id, token));
        vc_p->exp_rdback_time[token] = -1;
        // vc_p->acc_time[token] = st.timestamp;
        vc_p->acc_time[token] = at.cycle;
        // std::cout << "take token: " << token << std::endl;
        debug("take token %d\n", token);

        at.token = token;
    }
    return true;
}

// data out axi
void StaticVirtualChannel::step(uint64_t cycles, uint64_t &next_valid_rd_time, IdAllocator *ia, Log *log)
{
    // virtual channel schedule METHOD 1: RoundRobinArbiter
    VirtualChannel* vc = nullptr; 
    {
        static size_t last_i = 0;
        if (vcs.size() == 1) {
            if (!vcs[0]->rdback_buffer.empty()) {
                vc = vcs[0];
            }
        } else {
            size_t i = last_i;
            do {
                i = (i + 1) % vcs.size();
                if (!vcs[i]->rdback_buffer.empty()) {
                    vc = vcs[i];
                    last_i = i;
                    break;
                }
            } while (i != last_i);
        }
    }

    // virtual channel schedule METHOD 2: BalancedArbiter
    // VirtualChannel* vc = nullptr;
    // {
    //     std::vector<VirtualChannel*> longest_vcs;
    //     size_t max_len = 0;
    //     for (auto vc: vcs) {
    //         // ignore empty vcs
    //         if (vc->rdback_buffer.empty()) {
    //             continue;
    //         }
    //         // for all vcs with first-item-in-rdback_buffer returned
    //         auto& item = vc->rdback_buffer.front();
    //         TOKEN_TYPE token = item.second;
    //         assert(token < vc->exp_rdback_time.size());
    //         if (vc->exp_rdback_time[token] <= cycles) {
    //             // choose the ones with the longest rdback_buffer
    //             if (vc->rdback_buffer.size() > max_len) {
    //                 longest_vcs.clear();
    //                 longest_vcs.push_back(vc);
    //                 max_len = vc->rdback_buffer.size();
    //             } else if (vc->rdback_buffer.size() == max_len) {
    //                 longest_vcs.push_back(vc);
    //             } else {
    //                 continue;
    //             }
    //         } else {
    //             continue;
    //         }
    //     }
    //     // for all longest vcs, randomly choose one
    //     if (longest_vcs.size() > 0) {
    //         vc = longest_vcs[rand() % longest_vcs.size()];
    //     } else {
    //         vc = nullptr;
    //     }
    // }


    if (vc != nullptr) {
        assert(!vc->rdback_buffer.empty());
        auto& item = vc->rdback_buffer.front();
        ID_TYPE id = item.first;
        TOKEN_TYPE token = item.second;
        assert(token < vc->exp_rdback_time.size());
        if (vc->exp_rdback_time[token] <= cycles && next_valid_rd_time <= cycles) {
            uint64_t latency = cycles - vc->acc_time[token];
            vc->rd_latency[latency]++;
            stats.total_latency += latency;
            stats.read_count++;
            // dump latency
            if (DumpConfig::DUMP_LATENCY) {
                std::cout /*<< "returned token: " << token << ", latency: "*/<< latency << std::endl;
            }
            vc->rdback_buffer.pop();
            ia->release_rid(id);
            vc->available_tokens.push(token);
            next_valid_rd_time = cycles + interval;
            log->out_axi(id, token);
            // ++returned;
        }
    }
}

bool StaticVirtualChannel::all_done() const
{
    bool all_done = true;
    for (auto &vc: vcs) {
        all_done &= vc->rdback_buffer.empty();
    }
    return all_done;
}