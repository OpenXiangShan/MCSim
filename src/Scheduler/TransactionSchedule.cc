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

#include "top/Transaction.h"
#include "utils/debug.h"
#include "utils/FixedQueue.h"
#include "Scheduler/TransactionSchedule.h"
#include "Scg/SdramCommandGen.h"
#include "AXI2UI/AXI2UI.h"
#include <iostream>
#include "utils/log.h"

namespace {
constexpr bool PREFER_FILTER = true;
constexpr bool PREFER_CACHE = false;
constexpr bool PREFER_READ = true;
constexpr bool PREFER_WRITE = false;
} // namespace

static int get_idx(const MappedTransaction &t);

int TransactionSchedule::arb_input_index(ArbInput input)
{
    return static_cast<int>(input);
}

TransactionSchedule::ArbInput TransactionSchedule::get_arb_input(const MappedTransaction &trans, CallerType caller)
{
    bool from_cache = caller == CallerType::CACHE_MISS || caller == CallerType::CACHE_WRITEBACK;
    if (from_cache) {
        return trans.is_read ? TransactionSchedule::ArbInput::CACHE_RD : TransactionSchedule::ArbInput::CACHE_WR;
    }
    return trans.is_read ? TransactionSchedule::ArbInput::FILTER_RD : TransactionSchedule::ArbInput::FILTER_WR;
}

TransactionSchedule::TransactionSchedule(): grant_read(gr_delay, 0), rw_switch_cnt(0), rhold(0), whold(0), scg(nullptr), log(nullptr)
{
    for (int i = 0; i <= gr_delay; i++) {
        grant_read[i - gr_delay] = true;
    }
    switch (SchedulerConfig::station_org) {
        case StationOrg::SINGLE:
            station_num = 1;
            break;
        case StationOrg::PER_BANK:
            station_num = all_bank_num;
            break;
        case StationOrg::PER_BG:
            station_num = rank_num * bg_num;
            break;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            break;
    }
    transQ.reserve(station_num);
    arb_inputs.resize(station_num);
    arb_fired.resize(station_num, false);
    filter_fired.resize(station_num, false);
    cache_fired.resize(station_num, false);
    fc_prefer_filter.resize(station_num, PREFER_CACHE);
    filter_prefer_read.resize(station_num, PREFER_WRITE);
    cache_prefer_read.resize(station_num, PREFER_WRITE);
    filter_hold_prefer.resize(station_num, HoldPrefer::NONE);
    cache_hold_prefer.resize(station_num, HoldPrefer::NONE);
    int queue_per_station = all_bank_num / station_num;
    for (int i = 0; i < all_bank_num; ++i) {
        bank_free[i] = true;
    }
    bool *bf = &bank_free[0];
    for (int i = 0; i < station_num; ++i) {
        transQ.emplace_back(SchedulerConfig::scheduler_queue_size);

        rStation.emplace_back(SchedulerConfig::rstation_size, 3, SchedulerConfig::rstation_size - 2);
        wStation.emplace_back(SchedulerConfig::wstation_size, 2, SchedulerConfig::wstation_size);
        // Configure the last added stations
        auto &rStationBack = rStation.back();
        auto &wStationBack = wStation.back();

        rStationBack.set_grant(true);
        wStationBack.set_grant(false);

        rStationBack.bank_free = bf;
        wStationBack.bank_free = bf;

        rStationBack.scg = scg;
        wStationBack.scg = scg;

        // Move the bank_free pointer to the next station's range
        bf += queue_per_station;
    }
}

void TransactionSchedule::step()
{
    // r/w switch
    rw_schedule();

    for (auto &r : rStation) {
        r.step();
    }
    for (auto &w : wStation) {
        w.step();
    }

    dispatch_pending_inputs();

    for (int i = 0; i < station_num; ++i) {
        if (!transQ[i].empty()) {
            auto &cmd = transQ[i].front();
            if (rStation[i].check_conflict(cmd) || wStation[i].check_conflict(cmd)) {
                continue;
            }

            bool added = cmd.is_read ? rStation[i].addTrans(cmd) : wStation[i].addTrans(cmd);
            if (added) {
                transQ[i].pop(); // Remove the transaction from the queue if successfully added
            }
        }
    }

    update_dispatcher_idle_holds();
    reset_dispatcher_cycle_state();
}

void TransactionSchedule::reset_dispatcher_cycle_state()
{
    std::fill(arb_fired.begin(), arb_fired.end(), false);
    std::fill(filter_fired.begin(), filter_fired.end(), false);
    std::fill(cache_fired.begin(), cache_fired.end(), false);
}

void TransactionSchedule::update_dispatcher_idle_holds()
{
    for (int i = 0; i < station_num; ++i) {
        const auto &inputs = arb_inputs[i];
        if (!filter_fired[i] && filter_hold_prefer[i] == HoldPrefer::NONE) {
            if (!inputs[arb_input_index(ArbInput::FILTER_RD)].empty()) {
                filter_hold_prefer[i] = HoldPrefer::READ;
            } else if (!inputs[arb_input_index(ArbInput::FILTER_WR)].empty()) {
                filter_hold_prefer[i] = HoldPrefer::WRITE;
            }
        }

        if (!cache_fired[i] && cache_hold_prefer[i] == HoldPrefer::NONE) {
            if (!inputs[arb_input_index(ArbInput::CACHE_RD)].empty()) {
                cache_hold_prefer[i] = HoldPrefer::READ;
            } else if (!inputs[arb_input_index(ArbInput::CACHE_WR)].empty()) {
                cache_hold_prefer[i] = HoldPrefer::WRITE;
            }
        }
    }
}

int TransactionSchedule::choose_dispatcher_input(int idx) const
{
    const auto &inputs = arb_inputs[idx];
    auto valid = [&](ArbInput input) {
        return !inputs[arb_input_index(input)].empty();
    };

    int valid_count = 0;
    int only_valid = -1;
    for (int i = 0; i < arb_input_num; ++i) {
        if (!inputs[i].empty()) {
            valid_count++;
            only_valid = i;
        }
    }

    if (valid_count == 0) {
        return -1;
    }
    if (valid_count == 1) {
        return only_valid;
    }

    auto choose_rw = [](bool rd_valid, HoldPrefer hold_prefer, bool rw_prefer_read,
                        ArbInput rd_input, ArbInput wr_input) {
        if (hold_prefer == HoldPrefer::READ) {
            return arb_input_index(rd_input);
        }
        if (hold_prefer == HoldPrefer::WRITE) {
            return arb_input_index(wr_input);
        }
        return (rw_prefer_read == PREFER_READ && rd_valid) ?
            arb_input_index(rd_input) : arb_input_index(wr_input);
    };

    bool filter_valid = valid(ArbInput::FILTER_RD) || valid(ArbInput::FILTER_WR);
    bool cache_valid = valid(ArbInput::CACHE_RD) || valid(ArbInput::CACHE_WR);

    if (fc_prefer_filter[idx] == PREFER_FILTER) {
        if (filter_valid) {
            return choose_rw(valid(ArbInput::FILTER_RD), filter_hold_prefer[idx], filter_prefer_read[idx],
                             ArbInput::FILTER_RD, ArbInput::FILTER_WR);
        }
        return choose_rw(valid(ArbInput::CACHE_RD), cache_hold_prefer[idx], cache_prefer_read[idx],
                         ArbInput::CACHE_RD, ArbInput::CACHE_WR);
    }

    if (cache_valid) {
        return choose_rw(valid(ArbInput::CACHE_RD), cache_hold_prefer[idx], cache_prefer_read[idx],
                         ArbInput::CACHE_RD, ArbInput::CACHE_WR);
    }
    return choose_rw(valid(ArbInput::FILTER_RD), filter_hold_prefer[idx], filter_prefer_read[idx],
                     ArbInput::FILTER_RD, ArbInput::FILTER_WR);
}

void TransactionSchedule::update_dispatcher_prefer(int idx, ArbInput input)
{
    auto valid = [&](ArbInput in) {
        return !arb_inputs[idx][arb_input_index(in)].empty();
    };

    switch (input) {
        case ArbInput::FILTER_RD:
            filter_fired[idx] = true;
            fc_prefer_filter[idx] = PREFER_CACHE;
            filter_prefer_read[idx] = PREFER_WRITE;
            filter_hold_prefer[idx] = valid(ArbInput::FILTER_WR) ? HoldPrefer::WRITE : HoldPrefer::NONE;
            break;
        case ArbInput::FILTER_WR:
            filter_fired[idx] = true;
            fc_prefer_filter[idx] = PREFER_CACHE;
            filter_prefer_read[idx] = PREFER_READ;
            filter_hold_prefer[idx] = valid(ArbInput::FILTER_RD) ? HoldPrefer::READ : HoldPrefer::NONE;
            break;
        case ArbInput::CACHE_RD:
            cache_fired[idx] = true;
            fc_prefer_filter[idx] = PREFER_FILTER;
            cache_prefer_read[idx] = PREFER_WRITE;
            cache_hold_prefer[idx] = valid(ArbInput::CACHE_WR) ? HoldPrefer::WRITE : HoldPrefer::NONE;
            break;
        case ArbInput::CACHE_WR:
            cache_fired[idx] = true;
            fc_prefer_filter[idx] = PREFER_FILTER;
            cache_prefer_read[idx] = PREFER_READ;
            cache_hold_prefer[idx] = valid(ArbInput::CACHE_RD) ? HoldPrefer::READ : HoldPrefer::NONE;
            break;
    }
}

void TransactionSchedule::dispatch_pending_inputs()
{
    for (int idx = 0; idx < station_num; ++idx) {
        if (transQ[idx].full()) {
            continue;
        }

        int chosen = choose_dispatcher_input(idx);
        if (chosen < 0) {
            continue;
        }

        auto &slot = arb_inputs[idx][chosen];
        bool added = transQ[idx].push(slot.front());
        assert(added);
        update_dispatcher_prefer(idx, static_cast<ArbInput>(chosen));
        slot.pop_front();
        arb_fired[idx] = true;
    }
}

bool TransactionSchedule::try_add_to_dispatcher(const MappedTransaction &trans, ArbInput input)
{
    int idx = get_idx(trans);
    int input_idx = arb_input_index(input);
    auto &input_q = arb_inputs[idx][input_idx];

    if (input_q.size() >= arb_input_depth) {
        return false;
    }

    input_q.push_back(trans);
    return true;
}

void TransactionSchedule::rw_schedule()
{
    assert(gr_delay > 0);
    if (grant_read[-1] != grant_read[0]) {
        rw_switch_cnt++;
    }

    grant_read.advance();

    auto grant_read_use = grant_read.getWire(-gr_delay);
    auto grant_read_assign = grant_read.getWire(0);
    *grant_read_assign = *grant_read_use;

    for (auto &r : rStation) {
        r.set_grant(*grant_read_use);
    }
    for (auto &w : wStation) {
        w.set_grant(!*grant_read_use);
    }


    bool rconflict = std::any_of(rStation.begin(), rStation.end(), [](const auto &station) {
        return station.is_conflict();
    });
    bool wconflict = std::any_of(wStation.begin(), wStation.end(), [](const auto &station) {
        return station.is_conflict();
    });

    if (rconflict) {
        *grant_read_assign = true;
    } else if (wconflict) { // wconflict && !rconflict
        *grant_read_assign = false;
    } else { // !rconflict && !wconflict
        if (*grant_read_use) {
            if (rhold > 27) {
                bool rall_empty = std::all_of(rStation.begin(), rStation.end(), [](const auto &station) {
                    return station.empty();
                });
                bool wall_empty = std::all_of(wStation.begin(), wStation.end(), [](const auto &station) {
                    return station.empty();
                });
                bool walmost_full = std::any_of(wStation.begin(), wStation.end(), [](const auto &station) {
                    return station.almost_full();
                });
                if (SchedulerConfig::switch_to_w_when_r_empty) {
                    *grant_read_assign = !(walmost_full || (rall_empty && !wall_empty));
                } else {
                    *grant_read_assign = !walmost_full;
                }
            }
        } else {
            if (whold > 81) {
            // if (whold > 256) {
            // if (false) {
                *grant_read_assign = true;
            } else if (whold > 16) {
                bool walmost_empty = std::all_of(wStation.begin(), wStation.end(), [](const auto &station) {
                    return station.almost_empty();
                });
                // bool ralmost_full = std::any_of(rstation.begin(), rstation.end(), [](const auto &station) {
                //     return station.almost_full();
                // });
                *grant_read_assign = walmost_empty; //|| ralmost_full;
            }
        }
    }

    if (*grant_read_use) {
        rhold += 1;
        whold = 0;
    } else {
        whold += 1;
        rhold = 0;
    }
}

size_t TransactionSchedule::get_num(size_t bank_id) const
{
    int st_idx = bank_id / (all_bank_num / station_num);
    size_t rnum = rStation[st_idx].get_num(bank_id);
    size_t wnum = wStation[st_idx].get_num(bank_id);

    if (grant_read[-gr_delay]) {
        return rnum;
    } else {
        return wnum;
    }
}

bool TransactionSchedule::has_page_hit(size_t bank_id, size_t row) const
{
    int st_idx = bank_id / (all_bank_num / station_num);

    return rStation[st_idx].has_page_hit(bank_id, row) ||
           wStation[st_idx].has_page_hit(bank_id, row);
}

void TransactionSchedule::set_scg(SdramCommandGen *scg)
{
    this->scg = scg;
    for (auto &r : rStation) {
        r.scg = scg;
    }
    for (auto &w : wStation) {
        w.scg = scg;
    }
}

static int get_idx(const MappedTransaction &t)
{
    switch (SchedulerConfig::station_org) {
        case StationOrg::SINGLE:
            return 0;
        case StationOrg::PER_BANK:
            return (t.mapped_addr.rank * bg_num + t.mapped_addr.bg) * bank_per_bg + t.mapped_addr.bank;
        case StationOrg::PER_BG:
            return t.mapped_addr.rank * bg_num + t.mapped_addr.bg;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            return 0;
    }
}

rw_trans_type TransactionSchedule::addTrans(const MappedTransaction &r, const MappedTransaction &w, rw_trans_type type, CallerType caller)
{
    bool accept_rd = false;
    bool accept_wr = false;

    switch (type) {
        case RD_ONLY:
            assert(r.is_read);
            accept_rd = try_add_to_dispatcher(r, get_arb_input(r, caller));
            break;
        case WR_ONLY:
            assert(!w.is_read);
            accept_wr = try_add_to_dispatcher(w, get_arb_input(w, caller));
            break;
        case RD_WR:
            assert(r.is_read && !w.is_read);
            accept_rd = try_add_to_dispatcher(r, get_arb_input(r, caller));
            accept_wr = try_add_to_dispatcher(w, get_arb_input(w, caller));
            break;
    }

    if (accept_rd) {
        log->in_scheduler(r);
    }

    // Return the appropriate transaction type
    if (accept_rd && accept_wr) {
        return RD_WR;
    } else if (accept_rd) {
        debug("token %d\n", r.token);
        return RD_ONLY;
    } else if (accept_wr) {
        return WR_ONLY;
    } else {
        return NO_RW;
    }
}

void TransactionSchedule::notify(int bank_no)
{
    assert(!bank_free[bank_no]);
    bank_free[bank_no] = true;
}

void TransactionSchedule::statistics() const
{
    std::cout << "=============== SCHEDULER STATISTICS ================" << std::endl;
    // Sum up conflict counts for rStation
    int total_r_conflict = std::accumulate(rStation.begin(), rStation.end(), 0, [](int sum, const auto &station) {
        return sum + station.conflict_count;
    });

    // Sum up conflict counts for wStation
    int total_w_conflict = std::accumulate(wStation.begin(), wStation.end(), 0, [](int sum, const auto &station) {
        return sum + station.conflict_count;
    });

    // Calculate total conflicts
    int total_conflict = total_r_conflict + total_w_conflict;

    // Output the 
    std::cout << "| total conflict: " << total_conflict << std::endl;
    std::cout << "| rw_switch_cnt: " << rw_switch_cnt << std::endl;
}

void TransactionSchedule::resetStats()
{
    rw_switch_cnt = 0;
    // 重置所有 rStation 和 wStation 的 conflict_count
    for (auto &station : rStation) {
        station.conflict_count = 0;
    }
    for (auto &station : wStation) {
        station.conflict_count = 0;
    }
}

bool TransactionSchedule::all_done() const
{
    // transQ and rwStation are empty
    bool transQ_empty = std::all_of(transQ.begin(), transQ.end(), [](const auto &queue) {
        return queue.empty();
    });
    bool arb_empty = std::all_of(arb_inputs.begin(), arb_inputs.end(), [](const auto &inputs) {
        return std::all_of(inputs.begin(), inputs.end(), [](const auto &input_q) {
            return input_q.empty();
        });
    });
    bool rStation_empty = std::all_of(rStation.begin(), rStation.end(), [](const auto &station) {
        return station.empty();
    });
    bool wStation_empty = std::all_of(wStation.begin(), wStation.end(), [](const auto &station) {
        return station.empty();
    });
    
    return arb_empty && transQ_empty && rStation_empty && wStation_empty;
}