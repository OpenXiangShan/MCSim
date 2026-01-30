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

#include "top/Controller.h"

Controller::Controller(IdAllocator *ia) 
    : axi2ui(Axi2uiConfig::rob_size),   // tok_cap equals rob_size
      ft(filter_size),
      am(addr_map_size),
      cache(),
      cycles(59),
      scheduler(),
      scg(),
      log(DumpConfig::dump_log_dir_path)
{
    // axi2ui
    axi2ui.ft  = &ft;
    axi2ui.log = &log;
    axi2ui.ia = ia;
    // ft
    ft.am  = &am;
    ft.log = &log;
    // am
    am.scheduler = &scheduler;
    am.cache     = &cache;
    am.log       = &log;
    // cache
    cache.set_axi2ui(&axi2ui);
    cache.scheduler = &scheduler;
    // scheduler
    scheduler.set_scg(&scg);
    scheduler.axi2ui = &axi2ui;
    scheduler.log    = &log;
    // scg
    scg.set_axi2ui(&axi2ui);
    scg.scheduler = &scheduler;
    scg.cache     = &cache;
    scg.log       = &log;
    // log
    log.set_controller(this);
}

void Controller::statistics() const
{
    axi2ui.statistics();
    cache.statistics();
    scheduler.statistics();
    scg.statistics();
    std::cout << "=====================================================" << std::endl;
}

bool Controller::addTrans(SysTransaction& st)
{
    return axi2ui.addTrans(st);
}

TIME_TYPE Controller::get_cycles() const {return cycles;}

void Controller::step()
{
    ++cycles;
    scg.step();
    scheduler.step();
    cache.step();
    // am.step();
    ft.step();
    axi2ui.step();
}


bool CmdStation::is_conflict() const
{
    return conflict;
}

bool Controller::all_done() const
{
    // if axi2ui's read back is done, then all done 
    return axi2ui.all_done();// && ft.all_done() && am.all_done();
}