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

#include "Scg/RequestGenrator.h"
#include "utils/debug.h"
#include <iostream>
RequestGenerator::RequestGenerator(): transQ(1), state(QUERY_PAGE), block_req(false), release_req(false),
    act_resp(false), pre_resp(false), pre_req(false), act_req(false), cas_req(false), cas_resp(false), p({false, 0}) {}

void RequestGenerator::set_block()
{
    block_req = true; release_req = false;
}

void RequestGenerator::set_release()
{
    block_req = false; release_req = true;
}

bool RequestGenerator::addTrans(const MappedTransaction &trans)
{
    return transQ.push(trans);
}

bool RequestGenerator::blocked() const
{
    return state == BLOCKED;
}

void RequestGenerator::step()
{
// state machine
    switch (state) {
        case (QUERY_PAGE):
            if (block_req) {
                block_req = false;
                state = BLOCKED;
            } else if (!transQ.empty()) {
                auto cur = transQ.front();
                if (p.open) {
                    if (p.row == cur.mapped_addr.row) {
                        state = CAS;
                        cas_req = true;
                        cas_resp = false;
                    } else { // different row, close page first
                        p.open = false;
                        state = PRE;
                        pre_req = true;
                        pre_resp = false;
                    }
                } else { // closed page
                    p.row = cur.mapped_addr.row;   
                    p.open = true;
                    state = ACT;
                    act_req = true;
                    act_resp = false;
                }
            }
            break;
        case BLOCKED:
            if (release_req) {
                state = QUERY_PAGE;
                // close all pages
                p.open = false;
            }
            break;
        case PRE:
            if (pre_resp) {
                pre_req = false;
                state = QUERY_PAGE;
            }
            break;
        case ACT:
            if (act_resp) {
                act_req = false;
                state = CAS;
                cas_req = true;
                cas_resp = false;
            }
            break;
        case CAS:
            if (cas_resp) {
                cas_req = false;
                state = QUERY_PAGE;
                // std::cout << transQ.front().is_read << std::endl;
                transQ.pop();
                if (RowPolicy::CLOSE == row_policy) {
                    p.open = false;
                }
            }
            break;
    }
}