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

#ifndef TIMEING_CHECK_H
#define TIMEING_CHECK_H
#include "utils/common.h"
#include "config.h"
#include "top/Transaction.h"
#include <vector>
class TimingCheck {
public:
    void step(); // update timing based on dficmd
    TimingCheck();
    bool check_act(const MappedTransaction &act) const;
    bool check_pre(const MappedTransaction &pre) const;
    std::pair<CASFail, TIME_TYPE> check_cas(const MappedTransaction &cas) const;
    bool check_prea() const;
    void set_phase(const DFICommand *phase0, const DFICommand *phase1);
private:
    const DFICommand *phase0, *phase1;
    TIME_TYPE cycles;
    TIME_TYPE tRTW_exp;
    TIME_TYPE tRTPA_exp, tRASA_exp, tWRA_exp;
    std::vector<TIME_TYPE> tCCD_L_exp; // per bg
    std::vector<TIME_TYPE> tCCD_S_exp; // per bg
    std::vector<TIME_TYPE> tWTR_L_exp; // per bg
    std::vector<TIME_TYPE> tWTR_S_exp; // per bg
    std::vector<TIME_TYPE> tRRD_L_exp; // per bg
    std::vector<TIME_TYPE> tRRD_S_exp; // per bg
    std::vector<TIME_TYPE> tWR_exp; // per bank
    std::vector<TIME_TYPE> tRTP_exp; // per bank
    std::vector<TIME_TYPE> tRAS_exp; // per bank
    std::vector<TIME_TYPE> tRCD_exp; // per bank
    std::vector<TIME_TYPE> tRP_exp; // per bank
    std::vector<uint64_t> tFAW_window_rank;   // per rank
    std::vector<int> act_count_rank;   // per rank
    std::vector<TIME_TYPE> tW2WDR_exp; // per rank
    std::vector<TIME_TYPE> tW2RDR_exp; // per rank
    std::vector<TIME_TYPE> tR2RDR_exp; // per rank
    std::vector<TIME_TYPE> tR2WDR_exp; // per rank
    int last_actBG;
    int last_casBG;
};
#endif