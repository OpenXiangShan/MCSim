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

#ifndef ADDRMAP_H
#define ADDRMAP_H
#include "utils/FixedQueue.h"
#include "top/Transaction.h"
#include "utils/log.h"

class TransactionSchedule;
class Cache;
class Log;
class AddrMap {
private:
    FixedQueue<MappedTransaction> rTrans;
    FixedQueue<MappedTransaction> wTrans;
public:
    AddrMap(size_t capacity): rTrans(capacity), wTrans(capacity) {};
    rw_trans_type addTrans(const FilterTransaction &, const FilterTransaction &, rw_trans_type);
    rw_trans_type addTransComb(const FilterTransaction &, const FilterTransaction &, rw_trans_type);
    bool all_done() const;
    void step();
    TransactionSchedule *scheduler;
    Cache *cache;
    Log *log;
};

MappedAddr linear_to_mapped(uint64_t addr);

#endif