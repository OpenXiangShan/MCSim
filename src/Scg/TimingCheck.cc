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

#include "Scg/TimingCheck.h"
#include <assert.h>
TimingCheck::TimingCheck(): cycles(0), last_actBG(0), last_casBG(0)
{
    tRTW_exp = tRTPA_exp = tRASA_exp = tWRA_exp = 0;
    tCCD_L_exp.resize(bg_num, 0);
    tCCD_S_exp.resize(bg_num, 0);
    tRRD_L_exp.resize(bg_num, 0);
    tRRD_S_exp.resize(bg_num, 0);
    tWTR_L_exp.resize(bg_num, 0);
    tWTR_S_exp.resize(bg_num, 0);
    tWR_exp.resize(all_bank_num, 0);
    tRTP_exp.resize(all_bank_num, 0);
    tRAS_exp.resize(all_bank_num, 0);
    tRCD_exp.resize(all_bank_num, 0);
    tRP_exp.resize(all_bank_num, 0);
    tFAW_window_rank.resize(rank_num, 0);
    act_count_rank.resize(rank_num, 0);
    tW2WDR_exp.resize(rank_num, 0);
    tW2RDR_exp.resize(rank_num, 0);
    tR2RDR_exp.resize(rank_num, 0);
    tR2WDR_exp.resize(rank_num, 0);
}

bool TimingCheck::check_act(const MappedTransaction &act) const
{
    // return true;
    int bank_id = act.mapped_addr.bg * bank_per_bg + act.mapped_addr.bank;
    return cycles >= tRP_exp[bank_id] && act_count_rank[act.mapped_addr.rank] < 4 \
        && cycles >= tRRD_L_exp[act.mapped_addr.bg] && cycles >= tRRD_S_exp[act.mapped_addr.bg];
}

bool TimingCheck::check_pre(const MappedTransaction &pre) const
{
    int bank_id = pre.mapped_addr.bg * bank_per_bg + pre.mapped_addr.bank;
    return cycles >= tRTP_exp[bank_id] && cycles >= tRAS_exp[bank_id] && cycles >= tWR_exp[bank_id];
}

std::pair<CASFail, TIME_TYPE> TimingCheck::check_cas(const MappedTransaction &cas) const
{
    int bank_id = cas.mapped_addr.bg * bank_per_bg + cas.mapped_addr.bank;
    TIME_TYPE tCCD_exp =  std::max(tCCD_L_exp[cas.mapped_addr.bg], tCCD_S_exp[cas.mapped_addr.bg]);// cas.mapped_addr.bg == last_casBG ? tCCD_L_exp : tCCD_S_exp;
    // if (cas.is_read) {
    //     // TIME_TYPE tWTR_exp = tWTR_L_exp;
    //     TIME_TYPE tWTR_exp = cas.mapped_addr.bg == last_casBG ? tWTR_L_exp : tWTR_S_exp;
    //     return cycles >= tWTR_exp && cycles >= tRCD_exp[bank_id] && cycles >= tCCD_exp;
    // } else {
    //     return cycles >= tCCD_exp && cycles >= tRCD_exp[bank_id] && cycles >= tRTW_exp;
    // }
    // idle for row switch
    if (cycles < tRCD_exp[bank_id]) {
        return std::make_pair(CASFail::ROW_SWITCH, tRCD_exp[bank_id] - cycles);
    }

    // idle for rw switch
    if (cas.is_read) {
        TIME_TYPE tWTR_exp = std::max(tWTR_L_exp[cas.mapped_addr.bg], tWTR_S_exp[cas.mapped_addr.bg]); // cas.mapped_addr.bg == last_casBG ? tWTR_L_exp : tWTR_S_exp;
        if (cycles < tWTR_exp) {
            return std::make_pair(CASFail::W2R, tWTR_exp - cycles);
        }
        // dual rank check
        if (cycles < tW2RDR_exp[cas.mapped_addr.rank]) {
            return std::make_pair(CASFail::W2RDR, tW2RDR_exp[cas.mapped_addr.rank] - cycles);
        }
        if (cycles < tR2RDR_exp[cas.mapped_addr.rank]) {
            return std::make_pair(CASFail::R2RDR, tR2RDR_exp[cas.mapped_addr.rank] - cycles);
        }
    } else { // DFI_TYPE::WRITE
        if (cycles < tRTW_exp) {
            return std::make_pair(CASFail::R2W, tRTW_exp - cycles);
        }
        // dual rank check
        if (cycles < tR2WDR_exp[cas.mapped_addr.rank]) {
            return std::make_pair(CASFail::R2WDR, tR2WDR_exp[cas.mapped_addr.rank] - cycles);
        }
        if (cycles < tW2WDR_exp[cas.mapped_addr.rank]) {
            return std::make_pair(CASFail::W2WDR, tW2WDR_exp[cas.mapped_addr.rank] - cycles);
        }
    }

    // idle for bg conflict
    if (cycles < tCCD_exp) {
        return std::make_pair(CASFail::BG_CONFLICT, tCCD_exp - cycles);
    }
    return std::make_pair(CASFail::NONE, 0);
}

bool TimingCheck::check_prea() const
{
    return cycles >= tRTPA_exp && cycles >= tRASA_exp && cycles >= tWRA_exp;
}

void TimingCheck::step()
{
    ++cycles;
    // update timers
    // tFAW window
    bool has_act_phase0 = phase0->type == DFI_TYPE::ACT;
    bool has_act_phase1 = phase1->type == DFI_TYPE::ACT;
    assert(!(has_act_phase0 && has_act_phase1));
    uint8_t act_rank = has_act_phase0 ? phase0->rank : (has_act_phase1 ? phase1->rank : 0);
    for (int rank = 0; rank < rank_num; rank++) {
        bool has_act = (has_act_phase0 || has_act_phase1) ? (rank == act_rank) : false;
        act_count_rank[rank] = act_count_rank[rank] + has_act - ((tFAW_window_rank[rank] >> tFAW) & 1);
        tFAW_window_rank[rank] = (tFAW_window_rank[rank] << 1) | has_act;
    }

    // tRRD
    if (phase0->type == DFI_TYPE::ACT) {
        last_actBG = phase0->bg;
        tRRD_L_exp[phase0->bg] = cycles + tRRD_L;
        for (int i = 0; i < bg_num; ++i) {
            if (i != phase0->bg) {
                tRRD_S_exp[i] = cycles + tRRD_S;
            }
        }
    } else if (phase1->type == DFI_TYPE::ACT) {
        last_actBG = phase1->bg;
        tRRD_L_exp[phase1->bg] = cycles + tRRD_L + 1;
        for (int i = 0; i < bg_num; ++i) {
            if (i != phase1->bg) {
                tRRD_S_exp[i] = cycles + tRRD_S + 1;
            }
        }
    }
    // tCCD
    if (phase0->type == DFI_TYPE::WRITE || phase0->type == DFI_TYPE::READ) {
        last_casBG = phase0->bg;
        tCCD_L_exp[phase0->bg] = cycles + tCCD_L;
        for (int i = 0; i < bg_num; ++i) {
            if (i != phase0->bg) {
                tCCD_S_exp[i] = cycles + tCCD_S;
            }
        }
    } else if (phase1->type == DFI_TYPE::WRITE || phase1->type == DFI_TYPE::READ) {
        last_casBG = phase1->bg;
        tCCD_L_exp[phase1->bg] = cycles + tCCD_L + 1;
        for (int i = 0; i < bg_num; ++i) {
            if (i != phase1->bg) {
                tCCD_S_exp[i] = cycles + tCCD_S + 1;
            }
        }
    }
    // tWTR
    if (phase0->type == DFI_TYPE::WRITE) {
        tWTR_L_exp[phase0->bg] = cycles + tWTR_L;
        for (int i = 0; i < bg_num; ++i) {
            if (i != phase0->bg) {
                tWTR_S_exp[i] = cycles + tWTR_S;
            }
        }
    } else if (phase1->type == DFI_TYPE::WRITE) {
        tWTR_L_exp[phase1->bg] = cycles + tWTR_L + 1;
        for (int i = 0; i < bg_num; ++i) {
            if (i != phase1->bg) {
                tWTR_S_exp[i] = cycles + tWTR_S + 1;
            }
        }
    }
    // tRTW
    if (phase0->type == DFI_TYPE::READ) {
        tRTW_exp = cycles + tRTW;
    } else if (phase1->type == DFI_TYPE::READ) {
        tRTW_exp = cycles + tRTW + 1;
    }
    // tWR, per bank
    if (phase0->type == DFI_TYPE::WRITE) {
        int bank_id = phase0->bg * bank_per_bg + phase0->bank;
        tWR_exp[bank_id] = cycles + tWR;
        tWRA_exp = tWR_exp[bank_id];
    } else if (phase1->type == DFI_TYPE::WRITE) {
        int bank_id = phase1->bg * bank_per_bg + phase1->bank;
        tWR_exp[bank_id] = cycles + tWR + 1;
        tWRA_exp = tWR_exp[bank_id];
    }
    // tRTP, per bank
    if (phase0->type == DFI_TYPE::READ) {
        int bank_id = phase0->bg * bank_per_bg + phase0->bank;
        tRTP_exp[bank_id] = cycles + tRTP;
        tRTPA_exp = tRTP_exp[bank_id];
    } else if (phase1->type == DFI_TYPE::READ) {
        int bank_id = phase1->bg * bank_per_bg + phase1->bank;
        tRTP_exp[bank_id] = cycles + tRTP + 1;
        tRTPA_exp = tRTP_exp[bank_id];
    }
    // tRAS and tRCD, both per bank
    if (phase0->type == DFI_TYPE::ACT) {
        int bank_id = phase0->bg * bank_per_bg + phase0->bank;
        tRAS_exp[bank_id] = cycles + tRAS;
        tRCD_exp[bank_id] = cycles + tRCD;
        tRASA_exp = tRAS_exp[bank_id];
    } else if (phase1->type == DFI_TYPE::ACT) {
        int bank_id = phase1->bg * bank_per_bg + phase1->bank;
        tRAS_exp[bank_id] = cycles + tRAS + 1;
        tRCD_exp[bank_id] = cycles + tRCD + 1;
        tRASA_exp = tRAS_exp[bank_id];
    }
    // tRP, per bank
    if (phase0->type == DFI_TYPE::PRE) {
        int bank_id = phase0->bg * bank_per_bg + phase0->bank;
        tRP_exp[bank_id] = cycles + tRP;
    } else if (phase1->type == DFI_TYPE::PRE) {
        int bank_id = phase1->bg * bank_per_bg + phase1->bank;
        tRP_exp[bank_id] = cycles + tRP + 1;
    }
    // tW2WDR and tW2RDR, both per rank
    if (phase0->type == DFI_TYPE::WRITE) {
        for (int rank = 0; rank < rank_num; rank++) {
            if (rank != phase0->rank) {
                tW2WDR_exp[rank] = cycles + tW2WDR;
                tW2RDR_exp[rank] = cycles + tW2RDR;
            }
        }
    } else if (phase1->type == DFI_TYPE::WRITE) {
        for (int rank = 0; rank < rank_num; rank++) {
            if (rank != phase1->rank) {
                tW2WDR_exp[rank] = cycles + tW2WDR + 1;
                tW2RDR_exp[rank] = cycles + tW2RDR + 1;
            }
        }
    }
    // tR2WDR and tR2RDR, both per rank
    if (phase0->type == DFI_TYPE::READ) {
        for (int rank = 0; rank < rank_num; rank++) {
            if (rank != phase0->rank) {
                tR2WDR_exp[rank] = cycles + tR2WDR;
                tR2RDR_exp[rank] = cycles + tR2RDR;
            }
        }
    } else if (phase1->type == DFI_TYPE::READ) {
        for (int rank = 0; rank < rank_num; rank++) {
            if (rank != phase1->rank) {
                tR2WDR_exp[rank] = cycles + tR2WDR + 1;
                tR2RDR_exp[rank] = cycles + tR2RDR + 1;
            }
        }
    }
}

void TimingCheck::set_phase(const DFICommand *phase0, const DFICommand *phase1)
{
    this->phase0 = phase0;
    this->phase1 = phase1;
}