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

#ifndef LOG_H
#define LOG_H

#include "config.h"
#include "top/Transaction.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>

class Controller;
class Log
{
private:
    enum class log_type{
        AXI_IN,
        FILTER_IN,
        ADDRMAP_IN,
        SCHEDULER_IN,
        SCG_IN,
        FILTER_OUT,   // data back to rob
        AXI_OUT   // data out axi
    };
    std::string logDir;
    std::map<log_type, std::ofstream> logs_map;
    Controller* controller;
    bool log_disabled();
public:
    Log(const std::string& path);
    ~Log();
    void set_controller(Controller*);
    void in_axi(const AxiTransaction &rat);   // write cycles to Log when axi get in a rcmd
    void in_filter(const AxiTransaction &rat);   // write cycles to Log when filter get in a rcmd
    void in_addrmap(const FilterTransaction &rft);   // write cycles to Log when addrmap get in a rcmd
    void in_scheduler(const MappedTransaction &rmt);   // write cycles to Log when scheduler get in a rcmd
    void in_scg(const MappedTransaction &rmt);   // write cycles to Log when scg get in a rcmd
    void out_filter(const MappedTransaction &rmt);   // write cycles to Log when scg/scheduler/addrmap/filter get out a rcmd
    void out_axi(ID_TYPE id, TOKEN_TYPE token);   // write cycles to Log when axi get out a rcmd
};

#endif // LOG_H