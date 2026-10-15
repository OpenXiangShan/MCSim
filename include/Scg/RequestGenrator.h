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

#ifndef REQUEST_GENERATOR_H
#define REQUEST_GENERATOR_H
#include "utils/common.h"
#include "config.h"
#include <vector>
#include "top/Transaction.h"
#include "utils/FixedQueue.h"
class SdramCommandGen;
// generate act, pre, case request
class RequestGenerator {
public:
    RequestGenerator();
    size_t bank_id;   // index
    // FSM
    enum State{
        QUERY_PAGE,
        BLOCKED,
        PRE,
        ACT,
        CAS
    };
    struct page {
        bool open;
        uint16_t row;
    };
    void step();
    void set_block();
    void set_release();
    bool addTrans(const MappedTransaction &trans);
    bool blocked() const;
    bool block_req, release_req;
    bool pre_req, act_req, cas_req;
    bool act_resp, pre_resp, cas_resp;
    FixedQueue<MappedTransaction> transQ;
    SdramCommandGen *scg;
    // statistics
    struct {
        public:
            uint64_t row_hit_cnt = 0;
            uint64_t row_miss_cnt = 0;
            void row_hit() { if (!row_counted) { row_hit_cnt++; row_counted = true; } }
            void row_miss() { if (!row_counted) { row_miss_cnt++; row_counted = true; } }
            float row_hit_rate() const {
                if (row_hit_cnt + row_miss_cnt == 0)
                    return 0;
                return (float)row_hit_cnt / (row_hit_cnt + row_miss_cnt);
            }
            void cas_finish() { row_counted = false; }
        private:
            bool row_counted = false;
    } stats;
    class DY_PROF {
        public:
            // param
            static constexpr uint64_t T = 1000;   // clk for period
            static constexpr uint64_t t = 100;   // // clk for profiling during a period
            static constexpr float R = 0.2;   // row_hit_rate threshold
            // profiling info
            uint64_t p_i = 0;   // How many period has passed
            uint64_t last_row_hit = 0;   // How many row hit during all previous periods
            uint64_t last_row_miss = 0;   // How many row miss during all previous periods
            uint64_t period_row_hit = 0;
            uint64_t period_row_miss = 0;
            // result
            bool should_close = false;
            // functions
            void step(SdramCommandGen* scg, RequestGenerator* rg);
    } dr_prof;   // dynamic row profiling
    // MappedTransaction trans;
private:
    int close_row_timer = 0;
    bool should_close();
    page p;
    State state;
};
#endif