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

#ifndef PREFETCHER_H
#define PREFETCHER_H
#include <stdint.h>
#include "config.h"
#include "top/Transaction.h"
class Cache;
class Prefetcher {
protected:
    // reference config
    static constexpr const \
        CacheConfig::PrefetcherConfig &cfg = CacheConfig::pfc_cfg;
public:
    Cache *cache;
    virtual void step() = 0;
    virtual bool addTrans(const MappedTransaction &t) = 0;
};

#endif   // PREFETCHER_H