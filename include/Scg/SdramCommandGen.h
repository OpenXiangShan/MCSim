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

#ifndef DRAM_COMMAND_GEN_H
#define DRAM_COMMAND_GEN_H
#include "Scg/DFIPhaseFill.h"
#include "top/Transaction.h"
#include "utils/FixedQueue.h"
#include "Scg/BackGroundHold.h"
#include "Scg/RequestGenrator.h"
#include "Cache/Cache.h"
#include "config.h"
#include "Scg/TimingCheck.h"
#include <vector>
class AXI2UI;
class TransactionSchedule;
class Cache;
class Log;
// class DFIPhaseFill;
class SdramCommandGen {
private:
    std::vector<RequestGenerator> req_gen;
    BackGroundHold hold;
    TimingCheck timing_check;
    int last_act_no, last_pre_no, last_cas_no;
    DFICommand phase0, phase1;
    // statistics
    uint64_t idle_ref_zq, idle_rs, idle_r2w, idle_w2r, idle_bf, idle_empty, \
        idle_FAW, act_cnt, idle_w2wdr, idle_w2rdr, idle_r2rdr, idle_r2wdr;
    RequestGenerator *arb_act(bool &has_act, bool &has_act_req);
    RequestGenerator *arb_cas(bool &has_cas, bool &has_cas_req, \
        std::pair<CASFail, int> &cas_fail, const bool &has_act);
    RequestGenerator *arb_pre(bool &has_pre, bool &has_pre_req);
public:
    bool addTrans(const MappedTransaction &);
    void set_axi2ui(AXI2UI *axi2ui);
    SdramCommandGen();
    void step();
    void statistics() const;
    TransactionSchedule *scheduler;
    AXI2UI *axi2ui;
    Cache *cache;
    Log *log;
    // bool all_done();
};
#endif // DRAM_COMMAND_GEN_H