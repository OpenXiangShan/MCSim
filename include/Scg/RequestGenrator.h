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
// generate act, pre, case request
class RequestGenerator {
public:
    RequestGenerator();
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
    // MappedTransaction trans;
private:
    page p;
    State state;
};
#endif