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
#include "AddrMap/AddrMap.h"  // Include header for calculate_channel declaration
#include <array>

#ifdef GEM5_MODE
Controller::Controller(IdAllocator *ia)
    : log(DumpConfig::dump_log_dir_path)
#else
Controller::Controller(IdAllocator *ia, const std::string &log_dir)
    : log(log_dir)
#endif
{
    cycles = 59;
    readback_rr_next.fill(0);
    
    // Initialize all channels
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        channels[ch].axi2ui = new AXI2UI(cycles, Axi2uiConfig::rob_size, ch);
        channels[ch].ft = new Filter(filter_size);
        channels[ch].am = new AddrMap(addr_map_size);
        channels[ch].cache = new Cache();
        channels[ch].scg = new SdramCommandGen(cycles);
        channels[ch].scheduler = new TransactionSchedule();
        
        // Setup inter-component connections for this channel
        // axi2ui
        channels[ch].axi2ui->ft  = channels[ch].ft;
        channels[ch].axi2ui->log = &log;
        channels[ch].axi2ui->ia = ia;
        // ft
        channels[ch].ft->am  = channels[ch].am;
        channels[ch].ft->log = &log;
        // am
        channels[ch].am->scheduler = channels[ch].scheduler;
        channels[ch].am->cache     = channels[ch].cache;
        channels[ch].am->log       = &log;
        // cache
        channels[ch].cache->set_axi2ui(channels[ch].axi2ui);
        channels[ch].cache->controller = this;
        channels[ch].cache->scheduler = channels[ch].scheduler;
        channels[ch].cache->log = &log;
        // scheduler
        channels[ch].scheduler->set_scg(channels[ch].scg);
        channels[ch].scheduler->axi2ui = channels[ch].axi2ui;
        channels[ch].scheduler->log    = &log;
        // scg
        channels[ch].scg->set_axi2ui(channels[ch].axi2ui);
        channels[ch].scg->scheduler  = channels[ch].scheduler;
        channels[ch].scg->cache      = channels[ch].cache;
        channels[ch].scg->log        = &log;
        channels[ch].scg->controller = this;
    }
    // log
    log.set_controller(this);
}

Controller::~Controller()
{
    // Cleanup all channels
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        delete channels[ch].axi2ui;
        delete channels[ch].ft;
        delete channels[ch].am;
        delete channels[ch].cache;
        delete channels[ch].scg;
        delete channels[ch].scheduler;
    }
}

void Controller::statistics() const
{
    std::cout << "============= CONTROLLER STATISTICS =============" << std::endl;
    std::cout << "Total Channels: " << NUM_CHANNELS << std::endl;
    
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        std::cout << "\n--- Channel " << ch << " ---" << std::endl;
        channels[ch].axi2ui->statistics();
        channels[ch].scg->statistics();
        channels[ch].scheduler->statistics();
        channels[ch].cache->statistics();
    }
    
    std::cout << "=====================================================" << std::endl;
}

void Controller::resetStats()
{
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        channels[ch].axi2ui->resetStats();
        channels[ch].scg->resetStats();
        channels[ch].scheduler->resetStats();
        channels[ch].cache->resetStats();
    }
}

bool Controller::addTrans(const SysTransaction& st)
{
    // Calculate channel from address
    int ch = calculate_channel(st.addr);
    assert(ch >= 0 && ch < NUM_CHANNELS);
    
    // Add transaction to the appropriate channel
    return channels[ch].axi2ui->addTrans(st);
}

bool Controller::willAccept(uint64_t addr, bool is_write) const
{
    // 中文说明：mcsim 内部用 is_read 来区分，这里做一次转换。
    int ch = calculate_channel(addr);
    return channels[ch].axi2ui->willAccept(addr, !is_write);
}
 
TIME_TYPE Controller::get_cycles() const {return cycles;}

void Controller::arbitrate_readback(int ch)
{
    bool scg_valid = channels[ch].scg->hasReadyReadback();
    bool cache_valid = channels[ch].cache->hasReadyReadback();
    if (!scg_valid && !cache_valid) {
        return;
    }

    int chosen;
    if (scg_valid && cache_valid) {
        chosen = readback_rr_next[ch];
    } else {
        chosen = scg_valid ? 0 : 1;
    }

    if (chosen == 0) {
        assert(scg_valid);
        channels[ch].scg->commitReadyReadback();
    } else {
        assert(cache_valid);
        channels[ch].cache->commitReadyReadback();
    }
    readback_rr_next[ch] = 1 - chosen;
}

void Controller::step()
{
    ++cycles;
    
    // Step all channels in round-robin fashion
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        channels[ch].scg->step();
        channels[ch].scheduler->step();
        channels[ch].cache->step();
        arbitrate_readback(ch);
        channels[ch].ft->step();
        channels[ch].axi2ui->step();
    }
}

bool Controller::all_done() const
{
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        if (SchedulerConfig::switch_to_w_when_r_empty) {
            if (!(channels[ch].axi2ui->all_done() /* &&
                  channels[ch].ft->all_done() &&
                  channels[ch].am->all_done() &&
                  channels[ch].cache->all_done() &&
                  channels[ch].scheduler->all_done() &&
                  channels[ch].scg->all_done() */)) {
                return false;
            }
        } else {
            // If scheduler doesn't switch to w when r empty,
            // there will be entries remain in the wstation.
            // So we only check axi2ui(ROB) all done.
            if (!channels[ch].axi2ui->all_done()) {
                return false;
            }
        }
    }
    return true;
}