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
#include <vector>
class SdramCommandGen;
class AXI2UI;
class CmdStation;
class Log;

class TransactionSchedule {
    private:
        std::vector<FixedQueue<MappedTransaction>> transQ; // frontend
        std::vector<CmdStation> rStation, wStation;
        std::vector<bool> last_rd;
        bool bank_free[all_bank_num];
        bool grant_read;
        void rw_schedule();
        int rw_switch_cnt;
        int station_num;
    public:
        void notify(int bank_no);
        int rhold, whold;
        SdramCommandGen *scg;
        AXI2UI *axi2ui;
        void step();
        void statistics() const;
        TransactionSchedule();
        rw_trans_type addTrans(const MappedTransaction &, const MappedTransaction &, rw_trans_type, CallerType);
        size_t get_num(size_t bank_id);   // get the number of cmds in scheduler of specified bank
        void set_scg(SdramCommandGen *scg);
        Log *log;
};


#endif // TRANSACTION_SCHEDULE_H