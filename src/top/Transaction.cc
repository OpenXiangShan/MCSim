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

#include "top/Transaction.h"
#include <fstream>
#include <iostream>
std::istream& operator>> (std::istream& is, SysTransaction& trans)
{
    std::unordered_set<std::string> read_types = {"READ", "read", "r", "R"};
    std::string mem_op;
    is >> std::dec >>  trans.timestamp >> mem_op >> std::hex >> trans.addr;
    trans.is_read = (read_types.count(mem_op) == 1);
    // std::cout << " " << trans.timestamp << " 0x" << std::hex << trans.addr << std::endl << std::dec;
    return is;
}

std::ostream& operator<< (std::ostream& os, const MappedAddr& addr)
{
    os << "row: " << std::hex << addr.row << " col: " << addr.col << " bank: " << addr.bank << " bg: " << addr.bg;
    os << std::dec;
    return os;
}