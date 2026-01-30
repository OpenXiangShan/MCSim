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

#ifndef COMMON_H
#define COMMON_H
#include <cstdint>
#include <chrono>
#include <random>

typedef uint64_t TOKEN_TYPE;
typedef uint64_t TIME_TYPE;
typedef uint32_t ID_TYPE;
enum rw_trans_type {
    NO_RW,
    RD_ONLY,
    WR_ONLY,
    RD_WR
};
enum class CallerType {
    ROB,
    FILTER,
    ADDRMAP,
    CACHE_HIT,
    CACHE_MISS,
    CACHE_WRITEBACK,
    SCG,
    SCHEDULER
};
extern std::mt19937_64 rng;
#endif // COMMON_H