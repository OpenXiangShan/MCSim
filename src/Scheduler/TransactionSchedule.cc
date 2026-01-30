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
TransactionSchedule::TransactionSchedule(): scg(scg), grant_read(true), rw_switch_cnt(0), rhold(0), whold(0), bank_free(all_bank_num, true)
{
    switch (station_org) {
        case StationOrg::SINGLE:
            station_num = 1;
            break;
        case StationOrg::PER_BANK:
            station_num = all_bank_num;
            break;
        case StationOrg::PER_BG:
            station_num = bg_num;
            break;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            break;
    }
    last_rd.resize(station_num, false);
    transQ.reserve(station_num);
    int queue_per_station = all_bank_num / station_num;
    int rsize = rstation_size / station_num;
    int wsize = wstation_size / station_num;
    for (int i = 0; i < all_bank_num; ++i) {
        bank_free[i] = true;
    }
    bool *bf = &bank_free[0];
    for (int i = 0; i < station_num; ++i) {
        transQ.emplace_back(scheduler_queue_size);

        rStation.emplace_back(rsize, 3, rsize - 2);
        wStation.emplace_back(wsize, 2, wsize);
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
}

void TransactionSchedule::rw_schedule()
{
    // postpone by two cycles, next_grant_read[1] -> next_grant_read[0] -> grand_read
    static bool next_grant_read[2] = {true, true};
    for (auto &r : rStation) {
        r.set_grant(next_grant_read[0]);
    }
    for (auto &w : wStation) {
        w.set_grant(!next_grant_read[0]);
    }

    if (grant_read != next_grant_read[0]) {
        rw_switch_cnt++;
    }

    grant_read = next_grant_read[0];
    next_grant_read[0] = next_grant_read[1];

    bool rconflict = std::any_of(rStation.begin(), rStation.end(), [](const auto &station) {
        return station.is_conflict();
    });
    bool wconflict = std::any_of(wStation.begin(), wStation.end(), [](const auto &station) {
        return station.is_conflict();
    });

    if (rconflict) {
        next_grant_read[1] = true;
    } else if (wconflict) { // wconflict && !rconflict
        next_grant_read[1] = false;
    } else { // !rconflict && !wconflict
        if (grant_read) {
            if (rhold > 27) {
                bool walmost_full = std::any_of(wStation.begin(), wStation.end(), [](const auto &station) {
                    return station.almost_full();
                });
                next_grant_read[1] = !walmost_full;
            }
        } else {
            if (whold > 81 - 1) {
            // if (whold > 256) {
            // if (false) {
                next_grant_read[1] = true;
            } else if (whold > 16) {
                bool walmost_empty = std::all_of(wStation.begin(), wStation.end(), [](const auto &station) {
                    return station.almost_empty();
                });
                // bool ralmost_full = std::any_of(rstation.begin(), rstation.end(), [](const auto &station) {
                //     return station.almost_full();
                // });
                next_grant_read[1] = walmost_empty; //|| ralmost_full;
            }
        }
    }

    if (grant_read) {
        rhold += 1;
        whold = 0;
    } else {
        whold += 1;
        rhold = 0;
    }
}

size_t TransactionSchedule::get_num(size_t bank_id)
{
    int st_idx = bank_id % station_num;
    size_t rnum = rStation[st_idx].get_num(bank_id);
    size_t wnum = wStation[st_idx].get_num(bank_id);

    if (grant_read) {
        return rnum;
    } else {
        return wnum;
    }
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
    switch (station_org) {
        case StationOrg::SINGLE:
            return 0;
        case StationOrg::PER_BANK:
            return t.mapped_addr.bg * bank_per_bg + t.mapped_addr.bank;
        case StationOrg::PER_BG:
            return t.mapped_addr.bg;
        default:
            // Handle unexpected cases if necessary
            assert(0);
            return 0;
    }
}

rw_trans_type TransactionSchedule::addTrans(const MappedTransaction &r, const MappedTransaction &w, rw_trans_type type, CallerType caller)
{
    int ridx = get_idx(r);
    int widx = get_idx(w);

    bool accept_rd = false;
    bool accept_wr = false;

    // Helper function to attempt adding a transaction to the queue
    auto tryAddTransaction = [&](const MappedTransaction &trans, int idx, bool &accept) {
        accept = transQ[idx].push(trans);
    };

    switch (type) {
        case RD_ONLY:
            tryAddTransaction(r, ridx, accept_rd);
            break;
        case WR_ONLY:
            tryAddTransaction(w, widx, accept_wr);
            break;
        case RD_WR:
            if (last_rd[ridx]) {
                tryAddTransaction(w, widx, accept_wr);
            } else {
                tryAddTransaction(r, ridx, accept_rd);
            }
            break;
    }

    // Update last_rd based on accepted transactions
    if (accept_rd) {
        last_rd[ridx] = true;
        log->in_scheduler(r);
    }
    if (accept_wr) {
        last_rd[ridx] = false;
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