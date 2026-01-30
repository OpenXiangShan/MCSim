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

#ifndef __CMDSTATION_H__
#define __CMDSTATION_H__
#include "top/Transaction.h"
#include "Scg/SdramCommandGen.h"
#include "config.h"
#include <array>
#include "config.h"
#include <bitset>
#include <cassert>
class SdramCommandGen;
struct cmd_slot {
    MappedTransaction ts;
    cmd_slot *next, *prev;
};
class CmdStation {
public:
    CmdStation(int capcity, int l_water, int h_water);
    void step();
    void set_grant(bool g);
    bool addTrans(const MappedTransaction &trans);
    bool all_done() const;
    bool is_conflict() const;
    bool check_conflict(const MappedTransaction &trans);
    bool schedule_one(int bank_no);
    bool almost_full() const;
    bool almost_empty() const;
    bool empty() const;
    size_t get_num(size_t bank_id);   // get the number of cmds in CmdSt of specified bank
    int conflict_count;
    SdramCommandGen *scg;
    bool *bank_free;
    int queue_num;
private:
    bool grant;
    bool conflict;
    cmd_slot *conflict_slot;
    int size, capacity, l_water, h_water;
    cmd_slot *schedule_slot;
    std::vector<cmd_slot> valid_list;
    cmd_slot free_list;
    std::vector<cmd_slot *> valid_tail;
    std::vector<uint32_t> open_rows;
    // std::bitset<all_bank_num> bank_conflict;
    std::vector<bool> bank_conflict;
};

#endif // __CMDSTATION_H__