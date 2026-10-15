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

#ifndef MAINPIPE_H
#define MAINPIPE_H
#include "top/Transaction.h"
#include "config.h"
#include <array>

class Cache;
class CacheLine;
class MainPipeStage {
public:
    MainPipeStage(): valid(false), finish(false), block(false) {}
    bool valid;
    bool finish;
    MappedTransaction req;
    bool block;
    MappedTransaction block_req;   // when block, handle block_req first
    // S1 read CacheArray
    std::array<CacheLine*, CacheConfig::CACHE_WAYS> cachelines;
    // S2 generate way, is_hit, is_block, is_all_block etc.
    struct WayState {
        size_t way;
        bool is_hit, is_block, is_all_block, is_invalid, is_ready, is_evict;
    } way_state;
    // S3 write CacheArray

    bool can_receive() { return !valid || finish;}
    bool can_advance() { return valid && finish;}
};

class MainPipe {
private:
    MainPipeStage S1, S2, S3;
    struct {
        void init() {}
    } S1_info;
    struct {
        bool prefetch_done = false;
        bool urq_done = false;
        void init() {
            prefetch_done = false;
            urq_done = false;
        }
    } S2_info;
    struct {
        // TODO: S3_info
        void init() {}
    } S3_info;
    void advance_pipeline();
    void S1_step();
    void S2_step();
    void S3_step();

public:
    MainPipe();
    Cache *cache;
    bool addTrans(MappedTransaction req);
    struct CacheArrayWriteBus {
    public:
        bool valid;   // No ready bit needed here. If it is accepted, valid is then set false
        size_t index, bank, way;
        CacheLine *cacheline;
        void assign(size_t index, size_t bank, size_t way) {
            valid = true;
            this->index = index;
            this->bank = bank;
            this->way = way;
        }
    } writeBus;
    void step();
};

#endif   // MAINPIPE_H