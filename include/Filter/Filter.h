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

#ifndef FILTER_H
#define FILTER_H
#include "utils/FixedQueue.h"
#include "top/Transaction.h"
#include "utils/common.h"
#include "config.h"

class AddrMap;
class Log;
class Filter {
    private:
        FixedQueue<AxiTransaction> rTransQ, wTransQ;
        FilterTransaction do_filter(const AxiTransaction &);
    public:
        Filter(size_t capacity): rTransQ(capacity), wTransQ(capacity) {};
        rw_trans_type addTrans(const AxiTransaction &, const AxiTransaction &,  rw_trans_type);
        void step();
        bool all_done() const;
        AddrMap *am;
        Log *log;
};
#endif