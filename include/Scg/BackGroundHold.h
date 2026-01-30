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

#ifndef BACK_GROUND_HOLD_H
#define BACK_GROUND_HOLD_H
#include "config.h"
#include "utils/common.h"
class BackGroundHold {
public:
    enum State {
        IDLE,
        BLOCK,
        WAITPRE,
        RELEASE,
        PRECHARGE,
        REFRESH,
        ZQCS
    };
    void step();
    void set_all_clear(bool ac) { all_clear = ac; }
    bool get_all_clear() const { return all_clear; }
    bool is_blocked() const { return state == BLOCK; }
    bool is_released() const {return state == RELEASE;}
    bool is_wait() const {return state == WAITPRE;}
    bool is_idle() const {return state == IDLE;}
    bool busOccupy() const;
    BackGroundHold(): ref_timer(tREFI - 4330 - 3), zq_timer(0), pre_timer(0), rfc_timer(0), zqcs_timer(0),
        pend_ref(false), pend_zq(false), all_clear(false), state(IDLE) {}
private:
    int ref_timer, zq_timer;
    int pre_timer;
    int rfc_timer, zqcs_timer;
    bool pend_ref, pend_zq;
    bool all_clear;
    // TIME_TYPE cycles;
    State state;
};
#endif // BACK_GROUND_HOLD_H