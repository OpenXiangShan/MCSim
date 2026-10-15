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

#include "Scg/BackGroundHold.h"
#include <assert.h>
#include <iostream>
bool BackGroundHold::busOccupy() const {
    return state != IDLE && state != BLOCK && state != WAITPRE;
}

void BackGroundHold::step()
{
// state machine
    switch (state) {
        case IDLE:
            if(ref_pend_count > 0 || pend_zq) {
                state = BLOCK;
            }
            break;
        case BLOCK:
            state = WAITPRE;
            break;
        case WAITPRE:
            if (all_clear) {
                all_clear = false;
                state = PRECHARGE;
                pre_timer = 0;
            }
            break;
        case PRECHARGE:
            pre_timer++;
            if (pre_timer == tRP) {
                if (ref_pend_count > 0) {
                    --ref_pend_count;
                    state = REFRESH;
                    rfc_timer = 0;
                } else if (pend_zq) {
                    pend_zq = false;
                    state = ZQCS;
                    zqcs_timer = 0;
                } else {
                    std::cerr << "Error: unexpected state" << std::endl;
                    assert(0);
                }
            }
            break;
        case REFRESH:
            rfc_timer++;
            if (rfc_timer == tRFC) {
                if (ref_pend_count > 0) {
                    --ref_pend_count;
                    rfc_timer = 0;
                } else if (pend_zq) {
                    pend_zq = false;
                    state = ZQCS;
                    zqcs_timer = 0;
                } else {
                    state = RELEASE;
                }
            }
            break;
        case ZQCS:
            zqcs_timer++;
            if (zqcs_timer == tZQCS) {
                if (ref_pend_count > 0) {
                    --ref_pend_count;
                    state = REFRESH;
                    rfc_timer = 0;
                } else {
                    state = RELEASE;
                }
            }
            break;
        case RELEASE:
            state = IDLE;
            break;
    }

// timer increment
    ref_timer++;
    zq_timer++;
    switch (ScgConfig::ref_policy) {
        case RefreshPolicy::AUTO:
            if (ref_timer >= tREFI) {
                ref_timer = 0;
                ++ref_pend_count;
            }
            break;

        case RefreshPolicy::POSTPONED:
            if (ref_timer >= tREFI * 8) {
                ref_timer = 0;
                ref_pend_count += 8;
            }
            break;

        case RefreshPolicy::SPECULATIVE:
            // TODO: speculative refresh
            break;

        default:
            break;
    }

    if (zq_timer == tZQINTVL) {
        // zq_timer = 0;
        zq_timer = -1;
        // zqcs
        pend_zq = true;
    }
}