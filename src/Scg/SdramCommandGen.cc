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

#include "Scg/SdramCommandGen.h"
#include "Scg/DFIPhaseFill.h"
#include <iostream>
#include <fstream>
#include "utils/debug.h"
#include "Scheduler/TransactionSchedule.h"
#include "AXI2UI/AXI2UI.h" // Include the header for AXI2UI
#include "utils/log.h"

static void inline dump_as_scg_trans(const MappedTransaction &trans, int bg, int bank)
{
    if (DumpConfig::DUMP_AS_SCG_TRANS) {
        if (bg == 0 && bank == 0) {
            std::string ofname = "scheduler_scg_transaction.log";
            std::ofstream outFile(ofname, std::ios::app);
            if (outFile.is_open()) {
                outFile << (trans.is_read ? "READ, " : "WRITE, ") << std::hex << trans.addr << std::endl;
                outFile.close();
            } else {
                std::cerr << "Failed to open file " << ofname << std::endl;
            }
        } 
    }
}

static inline void dump_dfi_trans(const MappedTransaction &trans)
{
    // dump dfi r/w transaction
    if (DumpConfig::DUMP_DFI_RW_TRANS) {
        std::cout << (trans.is_read ? "R" : "W") \
            << ", 0x" << std::hex << trans.addr << std::dec << std::endl;
    }
    debug("casQ pop, token %d, addr %lx\n", trans.token, trans.addr);
}

bool SdramCommandGen::addTrans(const MappedTransaction &trans)
{
    int bg = trans.mapped_addr.bg;
    int bank = trans.mapped_addr.bank;
    // dump scheduler to scg transaction
    dump_as_scg_trans(trans, bg, bank);
    bool accepted = req_gen[bg * bank_per_bg + bank].addTrans(trans);
    if (trans.is_read && accepted) {
        log->in_scg(trans);
    }
    return accepted;
}

void SdramCommandGen::statistics() const
{
    std::cout << "================== SCG STATISTICS ===================" << std::endl;
    std::cout << "| idle_ref_zq: " << idle_ref_zq << std::endl;
    std::cout << "| idle_rs: " << idle_rs << std::endl;
    std::cout << "| idle_r2w: " << idle_r2w << std::endl;
    std::cout << "| idle_w2r: " << idle_w2r << std::endl;
    std::cout << "| idle_empty: " << idle_empty << std::endl;
    std::cout << "| idle_bf: " << idle_bf << std::endl;
    std::cout << "| idle_w2wdr: " << idle_w2wdr << std::endl;
    std::cout << "| idle_w2rdr: " << idle_w2rdr << std::endl;
    std::cout << "| idle_r2rdr: " << idle_r2rdr << std::endl;
    std::cout << "| idle_r2wdr: " << idle_r2wdr << std::endl;
    // std::cout << "idle_FAW: " << idle_FAW << std::endl;
    std::cout << "| all idle cycles: " << idle_ref_zq + idle_rs + idle_r2w + idle_w2r + \
        idle_w2wdr + idle_w2rdr + idle_r2rdr + idle_r2wdr + idle_bf + idle_empty << std::endl;
    std::cout << "| act_cnt: " << act_cnt << std::endl;
}


void SdramCommandGen::set_axi2ui(AXI2UI *axi2ui)
{
    this->axi2ui = axi2ui;
}

SdramCommandGen::SdramCommandGen()
{
    for (int i = 0; i < all_bank_num; ++i) {
        req_gen.push_back(RequestGenerator());
    }
    timing_check.set_phase(&(phase0), &(phase1));
    last_act_no = -1, last_pre_no = -1, last_cas_no = -1;
    idle_ref_zq = idle_rs = idle_r2w = idle_w2r = idle_bf = idle_empty = \
        idle_FAW = act_cnt = idle_w2wdr = idle_w2rdr = idle_r2rdr = idle_r2wdr = 0;
    phase0.type = DFI_TYPE::NONE;
    phase1.type = DFI_TYPE::NONE;
}

RequestGenerator *SdramCommandGen::arb_act(bool &has_act, bool &has_act_req)
{
    // METHOD 1: RoundRobinArbiter
    if (ScgConfig::arb_policy == ScgArbPolicy::RoundRobin) {
        for (int i = 1; i <= all_bank_num; ++i) {
            int idx = (i + last_act_no) % all_bank_num;
            auto &gen = req_gen[idx];
            if (gen.act_req) {
                has_act_req = true;
                if (timing_check.check_act(gen.transQ.front())) {
                    has_act = true;
                    last_act_no = idx;
                    return &gen;
                }
            }
        }
        return nullptr;
    }

    // METHOD 2: BalancedArbiter
    if (ScgConfig::arb_policy == ScgArbPolicy::Balanced) {
        std::vector<RequestGenerator *> act_req_gens;
        std::vector<int> hungry_check_idxes;
        RequestGenerator *gen_act;
        size_t max_act_num = 0;
        static int wait_time[all_bank_num] = {0};
        for (int i = 0; i < all_bank_num; ++i) {
            int idx = i % all_bank_num;
            auto &gen = req_gen[idx];
            // for all req_gen with act and timing_check is ok
            if (gen.act_req) {
                has_act_req = true;
                if (timing_check.check_act(gen.transQ.front())) {
                    // choose the banks with the most cmds in scheduler
                    has_act = true;
                    wait_time[idx]++;
                    hungry_check_idxes.push_back(idx);
                    if (scheduler->get_num(idx) > max_act_num) {
                        act_req_gens.clear();
                        act_req_gens.push_back(&gen);
                        max_act_num = scheduler->get_num(idx);
                    } else if (scheduler->get_num(idx) == max_act_num) {
                        act_req_gens.push_back(&gen);
                    }
                }
            }
        }
        // if one bank wait too long, choose it
        for (auto &i: hungry_check_idxes) {
            if (wait_time[i] > ScgConfig::hungry_time) {
                assert(req_gen[i].act_req && timing_check.check_act(req_gen[i].transQ.front()));
                wait_time[i] = 0;
                return &req_gen[i];
            }
        }
        // for all act_req_gens, randomly choose one
        if (act_req_gens.size() > 0) {
            int i = rand() % act_req_gens.size();
            gen_act = act_req_gens[i];
            wait_time[gen_act - &req_gen[0]] = 0;
            return gen_act;
        } else {
            return nullptr;
        }
    }

    std::cout << "Scg set unknown arb policy." << std::endl;
    exit(1);
}

RequestGenerator *SdramCommandGen::arb_cas(bool &has_cas, bool &has_cas_req, \
    std::pair<CASFail, int> &cas_fail, const bool &has_act)
{
    // METHOD 1: RoundRobinArbiter
    if (ScgConfig::arb_policy == ScgArbPolicy::RoundRobin) {
        for (int i = 1; i <= all_bank_num; ++i) {
            auto idx = (i + last_cas_no) % all_bank_num;
            auto &gen = req_gen[idx];
            if (gen.cas_req) {
                has_cas_req = true;
                auto check_res = timing_check.check_cas(gen.transQ.front());
                if (check_res.first == CASFail::NONE) {
                    has_cas = true;
                    last_cas_no = idx;
                    return &gen;
                }
                if (check_res.second < cas_fail.second) {
                    cas_fail = check_res;
                }
            }
        }
        return nullptr;
    }

    // METHOD 2: BalancedArbiter
    if (ScgConfig::arb_policy == ScgArbPolicy::Balanced) {
        std::vector<RequestGenerator *> cas_req_gens;
        std::vector<int> hungry_check_idxes;
        RequestGenerator *gen_cas;
        size_t max_cas_num = 0;
        static int wait_time[all_bank_num] = {0};
        for (int i = 0; i < all_bank_num; ++i) {
            auto idx = i % all_bank_num;
            auto &gen = req_gen[idx];
            // for all req_gen with act and timing_check is ok
            if (gen.cas_req) {
                has_cas_req = true;
                auto check_res = timing_check.check_cas(gen.transQ.front());
                if (check_res.first == CASFail::NONE) {
                    // choose the banks with the most cmds in scheduler
                    has_cas = true;
                    wait_time[idx]++;
                    hungry_check_idxes.push_back(idx);
                    if (scheduler->get_num(idx) > max_cas_num) {
                        cas_req_gens.clear();
                        cas_req_gens.push_back(&gen);
                        max_cas_num = scheduler->get_num(idx);
                    } else if (scheduler->get_num(idx) == max_cas_num) {
                        cas_req_gens.push_back(&gen);
                    }
                } else if (check_res.second < cas_fail.second) {
                    cas_fail = check_res;
                }
            }
        }
        // if one bank wait too long, choose it
        for (auto &i: hungry_check_idxes) {
            if (wait_time[i] > ScgConfig::hungry_time) {
                assert(req_gen[i].cas_req && \
                    timing_check.check_cas(req_gen[i].transQ.front()).first == CASFail::NONE);
                wait_time[i] = 0;
                return &req_gen[i];
            }
        }
        // for all cas_req_gens, randomly choose one
        if (cas_req_gens.size() > 0) {
            int i = rand() % cas_req_gens.size();
            gen_cas = cas_req_gens[i];
            wait_time[gen_cas - &req_gen[0]] = 0;
            return gen_cas;
        } else {
            return nullptr;
        }
    }

    std::cout << "Scg set unknown arb policy." << std::endl;
    exit(1);
}

RequestGenerator *SdramCommandGen::arb_pre(bool &has_pre, bool &has_pre_req)
{
    // METHOD 1: RoundRobinArbiter
    if (ScgConfig::arb_policy == ScgArbPolicy::RoundRobin) {
        for (int i = 1; i <= all_bank_num; ++i) {
            auto idx = (i + last_pre_no) % all_bank_num;
            auto &gen = req_gen[idx];
            if (gen.pre_req) {
                // TODO() check timing, gen.ras
                has_pre_req = true;
                if (timing_check.check_pre(gen.transQ.front())) {
                    has_pre = true;
                    last_pre_no = idx;
                    return &gen;
                }
            }
        }
        return nullptr;
    }

    // METHOD 2: BalancedArbiter
    if (ScgConfig::arb_policy == ScgArbPolicy::Balanced) {
        std::vector<RequestGenerator *> pre_req_gens;
        std::vector<int> hungry_check_idxes;
        RequestGenerator *gen_pre;
        size_t max_pre_num = 0;
        static int wait_time[all_bank_num] = {0};
        for (int i = 0; i < all_bank_num; ++i) {
            auto idx = i % all_bank_num;
            auto &gen = req_gen[idx];
            // for all req_gen with act and timing_check is ok
            if (gen.pre_req) {
                // TODO() check timing, gen.ras
                has_pre_req = true;
                if (timing_check.check_pre(gen.transQ.front())) {
                    // choose the banks with the most cmds in scheduler
                    has_pre = true;
                    wait_time[idx]++;
                    hungry_check_idxes.push_back(idx);
                    if (scheduler->get_num(idx) > max_pre_num) {
                        pre_req_gens.clear();
                        pre_req_gens.push_back(&gen);
                        max_pre_num = scheduler->get_num(idx);
                    } else if (scheduler->get_num(idx) == max_pre_num) {
                        pre_req_gens.push_back(&gen);
                    }
                }
            }
        }
        // if one bank wait too long, choose it
        for (auto &i: hungry_check_idxes) {
            if (wait_time[i] > ScgConfig::hungry_time) {
                assert(req_gen[i].pre_req && timing_check.check_pre(req_gen[i].transQ.front()));
                wait_time[i] = 0;
                return &req_gen[i];
            }
        }
        if (pre_req_gens.size() > 0) {
            int i = rand() % pre_req_gens.size();
            gen_pre = pre_req_gens[i];
            wait_time[gen_pre - &req_gen[0]] = 0;
            return gen_pre;
        } else {
            return nullptr;
        }
    }
    
    std::cout << "Scg set unknown arb policy." << std::endl;
    exit(1);
}

void SdramCommandGen::step()
{
    timing_check.step();

    static bool last_has_act = false, last_has_cas = false, last_has_pre = false;
    phase0.type = DFI_TYPE::NONE;
    phase1.type = DFI_TYPE::NONE;

// arbitrate act
    bool has_act = false, has_act_req = false;
    RequestGenerator *gen_act = arb_act(has_act, has_act_req);
    if (gen_act != nullptr) {
        assert(!gen_act->act_resp);
        gen_act->act_resp = true;
        auto act = gen_act->transQ.front();
        phase0.assign(DFI_TYPE::ACT, act);
        ++act_cnt;
    }

// arbitrate cas
    bool has_cas = false, has_cas_req = false;
    auto cas_fail = std::make_pair(CASFail::NONE, 1000000);
    // if (!last_has_cas)
    if (hold.busOccupy()) {
        ++idle_ref_zq;
    } else if (!(ScgConfig::sphase && has_act)) {
        RequestGenerator *gen_cas = arb_cas(has_cas, has_cas_req, cas_fail, has_act);
        if (gen_cas != nullptr) {
            gen_cas->cas_resp = true;
            auto cas = gen_cas->transQ.front();
            dump_dfi_trans(cas);
            scheduler->notify(cas.mapped_addr.bg * bank_per_bg + cas.mapped_addr.bank);
            DFI_TYPE type = cas.is_read ? DFI_TYPE::READ : DFI_TYPE::WRITE;
            if (has_act) {
                phase1.assign(type, cas);
            } else {
                phase0.assign(type, cas);
            }
            if (cas.is_read) {
                if (cas.is_to_cache) {
                    cache->returnFromScg(cas);
                } else {
                    axi2ui->put_back(cas.id, cas.token, CallerType::SCG);
                    log->out_filter(cas);   // we merge read back path "scg,scheduler,addrmap,filter"
                }
            }
        }
    }

// arbitrate pre
    bool has_pre = false, has_pre_req = false;
    // if (!last_has_pre)
    if (!(!ScgConfig::sphase && (has_act && has_cas) || ScgConfig::sphase && (has_act || has_cas))) {
        RequestGenerator *gen_pre = arb_pre(has_pre, has_pre_req);
        if (gen_pre != nullptr) {
            assert(!gen_pre->pre_resp);
            gen_pre->pre_resp = true;
            auto pre = gen_pre->transQ.front();
            if (has_act || has_cas) {
                phase1.assign(DFI_TYPE::PRE, pre);
            } else {
                phase0.assign(DFI_TYPE::PRE, pre);
            }
        }
    }

    if (!has_cas_req && !hold.busOccupy()) {
        if (!has_act_req && !has_pre_req) {
            ++idle_empty; // no request, idle
        } else {
            ++idle_rs; // row switch
        }
    }

    if (has_cas_req && !has_cas) {
        // row switch, r2w, w2r, bank conflict, bg conflict
        switch (cas_fail.first) {
            case CASFail::ROW_SWITCH:
                ++idle_rs;
                break;
            case CASFail::W2R:
                ++idle_w2r;
                break;
            case CASFail::R2W:
                ++idle_r2w;
                break;
            case CASFail::BG_CONFLICT:
                ++idle_bf;
                break;
            case CASFail::W2WDR:
                ++idle_w2wdr;
                break;
            case CASFail::W2RDR:
                ++idle_w2rdr;
                break;
            case CASFail::R2RDR:
                ++idle_r2rdr;
                break;
            case CASFail::R2WDR:
                ++idle_r2wdr;
                break;
            default:
                break;
        }
    }
    
    
    hold.step();
    for (auto &gen: req_gen) {
        gen.step();
    }

    if (hold.is_blocked()) {
        for (auto &gen: req_gen) {
            gen.set_block();
        }
    }

    if (hold.is_wait()) {
        {
            bool all_clear = timing_check.check_prea();
            for (auto &gen: req_gen) {
                if (!all_clear) break;
                all_clear &= gen.blocked();
            }
            hold.set_all_clear(all_clear);
        }

    }

    if (hold.is_released()) {
        for (auto &gen: req_gen) {
            gen.set_release();
        }
    }
    last_has_act = has_act;
    last_has_cas = has_cas;
    last_has_pre = has_pre;
}