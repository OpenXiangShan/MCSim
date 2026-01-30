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

#include "utils/log.h"
#include "top/Controller.h"

namespace fs = std::filesystem;

bool Log::log_disabled()
{
    return DumpConfig::DUMP_MODULEWISE_LOG == false;
}

Log::Log(const std::string& path)
{
    if (log_disabled()) {
        return;
    }

    logDir = (path != "") ? path : "./logs";
    std::string inLogNames[] = {
        "axi_in_cycle.txt",
        "filter_in_cycle.txt",
        "addrmap_in_cycle.txt",
        "scheduler_in_cycle.txt",
        "scg_in_cycle.txt"
    };
    std::string outLogNames[] = {
        "filter_out_cycle.txt",
        "axi_out_cycle.txt"
    };

    // create in_cycle and out_cycle directories
    try {
        fs::create_directories(logDir + "/in_cycle");
        fs::create_directories(logDir + "/out_cycle");
    } catch (const fs::filesystem_error& e) {
        std::cout << "Error creating log directory: " << e.what() << std::endl;
        return;
    }

    // open file (if not exist, create it)
    try {
        logs_map[log_type::AXI_IN] = std::ofstream(logDir + "/in_cycle/" + inLogNames[0]);
        logs_map[log_type::FILTER_IN] = std::ofstream(logDir + "/in_cycle/" + inLogNames[1]);
        logs_map[log_type::ADDRMAP_IN] = std::ofstream(logDir + "/in_cycle/" + inLogNames[2]);
        logs_map[log_type::SCHEDULER_IN] = std::ofstream(logDir + "/in_cycle/" + inLogNames[3]);
        logs_map[log_type::SCG_IN] = std::ofstream(logDir + "/in_cycle/" + inLogNames[4]);
        logs_map[log_type::FILTER_OUT] = std::ofstream(logDir + "/out_cycle/" + outLogNames[0]);
        logs_map[log_type::AXI_OUT] = std::ofstream(logDir + "/out_cycle/" + outLogNames[1]);
    } catch (const fs::filesystem_error& e) {
        std::cout << "Error opening log file: " << e.what() << std::endl;
    }

    for (auto &log : logs_map) {
        if (!log.second.is_open()) {
            std::cerr << "Failed to open log file for type " << static_cast<int>(log.first) << std::endl;
        }
    }
    
    // write header
    logs_map[log_type::AXI_IN] << "cycle,araddr,id" << std::endl;
    logs_map[log_type::FILTER_IN] << "cycle,addr,id,token" << std::endl;
    logs_map[log_type::ADDRMAP_IN] << "cycle,addr,id,token" << std::endl;
    logs_map[log_type::SCHEDULER_IN] << "cycle,id,token,rank,bg,bank,row,col" << std::endl;
    logs_map[log_type::SCG_IN] << "cycle,id,token,cmdtype,rank,bg,bank,row,col" << std::endl;
    logs_map[log_type::FILTER_OUT] << "cycle,id,token" << std::endl;
    logs_map[log_type::AXI_OUT] << "cycle,id,token" << std::endl;

    // change ofstream to append mode
    for (auto& log : logs_map) {
        // log.second << std::ios::app;
    }
}

Log::~Log()
{
    if (log_disabled()) {
        return;
    }

    // close file
    for (auto& log : logs_map) {
        log.second.close();
    }
}

void Log::set_controller(Controller* controller)
{
    if (log_disabled()) {
        return;
    }
    
    this->controller = controller;
}

void Log::in_axi(const AxiTransaction &rat)
{
    if (log_disabled()) {
        return;
    }

    logs_map[log_type::AXI_IN] << std::hex << controller->get_cycles() << "," << rat.addr << "," << rat.id << std::endl;
}

void Log::in_filter(const AxiTransaction &rat)
{
    if (log_disabled()) {
        return;
    }

    logs_map[log_type::FILTER_IN] << std::hex << controller->get_cycles() << "," << rat.addr << "," << rat.id \
        << "," << rat.token << std::endl;
}

void Log::in_addrmap(const FilterTransaction &rft)
{
    if (log_disabled()) {
        return;
    }

    logs_map[log_type::ADDRMAP_IN] << std::hex << controller->get_cycles() << "," << rft.addr << "," << rft.id \
        << "," << rft.token << std::endl;
}

void Log::in_scheduler(const MappedTransaction &rmt)
{
    
    if (log_disabled()) {
        return;
    }

    logs_map[log_type::SCHEDULER_IN] << std::hex << controller->get_cycles() << "," << rmt.id << "," << rmt.token << "," \
        << 0 << "," << rmt.mapped_addr.bg << "," << rmt.mapped_addr.bank << "," \
        << rmt.mapped_addr.row << "," << rmt.mapped_addr.col << std::endl;
}

void Log::in_scg(const MappedTransaction &rmt)
{
    if (log_disabled()) {
        return;
    }

    logs_map[log_type::SCG_IN] << std::hex << controller->get_cycles() << "," << rmt.id << "," << rmt.token << "," \
        << rmt.is_read << "," \
        << 0 << "," << rmt.mapped_addr.bg << "," << rmt.mapped_addr.bank << "," \
        << rmt.mapped_addr.row << "," << rmt.mapped_addr.col << std::endl;
}

void Log::out_filter(const MappedTransaction &rmt)
{
    if (log_disabled()) {
        return;
    }

    logs_map[log_type::FILTER_OUT] << std::hex << controller->get_cycles() << "," << rmt.id << "," << rmt.token << std::endl;
}

void Log::out_axi(ID_TYPE id, TOKEN_TYPE token)
{
    if (log_disabled()) {
        return;
    }
    
    logs_map[log_type::AXI_OUT] << std::hex << controller->get_cycles() << "," << id << "," << token << std::endl;
}