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

#ifndef TRANSACTION_SCHEDULE_H
#define TRANSACTION_SCHEDULE_H
#include "config.h"
#include "Scheduler/CmdStation.h"
#include "utils/timebuf.h"
#include <algorithm>
#include <array>
#include <deque>
#include <vector>
class SdramCommandGen;
class AXI2UI;
class CmdStation;
class Log;

class TransactionSchedule {
    private:
        std::vector<FixedQueue<MappedTransaction>> transQ; // frontend
        std::vector<CmdStation> rStation, wStation;
        bool bank_free[all_bank_num];
        gem5::TimeBuffer<bool> grant_read;
        static constexpr int gr_delay = 1;
        static constexpr int arb_input_num = 4;
        static constexpr int arb_input_depth = 2;
        enum class ArbInput : int {
            FILTER_RD = 0,
            FILTER_WR = 1,
            CACHE_RD = 2,
            CACHE_WR = 3
        };
        enum class HoldPrefer : int {
            NONE = 0,
            READ = 1,
            WRITE = 2
        };
        std::vector<std::array<std::deque<MappedTransaction>, arb_input_num>> arb_inputs;
        std::vector<bool> arb_fired, filter_fired, cache_fired;
        std::vector<bool> fc_prefer_filter, filter_prefer_read, cache_prefer_read;
        std::vector<HoldPrefer> filter_hold_prefer, cache_hold_prefer;
        void rw_schedule();
        static int arb_input_index(ArbInput input);
        static ArbInput get_arb_input(const MappedTransaction &trans, CallerType caller);
        bool try_add_to_dispatcher(const MappedTransaction &trans, ArbInput input);
        int choose_dispatcher_input(int idx) const;
        void dispatch_pending_inputs();
        void update_dispatcher_prefer(int idx, ArbInput input);
        void update_dispatcher_idle_holds();
        void reset_dispatcher_cycle_state();
        int rw_switch_cnt;
        int station_num;
    public:
        void notify(int bank_no);
        int rhold, whold;
        SdramCommandGen *scg;
        AXI2UI *axi2ui;
        void step();
        void statistics() const;
        void resetStats();
        TransactionSchedule();
        rw_trans_type addTrans(const MappedTransaction &, const MappedTransaction &, rw_trans_type, CallerType);
        size_t get_num(size_t bank_id) const;   // get the number of cmds in scheduler of specified bank
        bool has_page_hit(size_t bank_id, uint64_t row) const;
        void set_scg(SdramCommandGen *scg);
        bool all_done() const;
        Log *log;
};


#endif // TRANSACTION_SCHEDULE_H