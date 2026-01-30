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

#ifndef CONFIG_H
#define CONFIG_H
#include <cstdint>
#define CONVERT_DDR_CYCLE(a) (((a + 1) >> 1) - 1)

// timing parameters
constexpr int freq = 600;   // MHz
class TimingConfig {
    public:
        static constexpr int tREFI = 7500;   // ns
        // static constexpr int tREFI = 999999999;   // no refresh
        static constexpr int tZQINTVL = 128;   // ms
        static constexpr int tRP = 22;   // ddr_cycle
        static constexpr int tRFC = 650;   // ns
        static constexpr int tZQCS = 128;   // mc_cycle
        static constexpr int tRRD_S = 4;   // ddr_cycle
        static constexpr int tRRD_L = 8;   // ddr_cycle
        static constexpr int tCCD_L = 8;   // ddr_cycle
        static constexpr int tCCD_S= 4;   // ddr_cycle
        static constexpr int tWTR_L = 32;   // ddr_cycle
        static constexpr int tWTR_S = 24;   // ddr_cycle
        static constexpr int BL = 8;   // ddr_cycle
        static constexpr int WL = 12;   // ddr_cycle
        static constexpr int tRTW = 10;   // ddr_cycle
        static constexpr int tWR = 24;   // ddr_cycle
        static constexpr int tRTP = 10;   // ddr_cycle
        static constexpr int tRAS = 54;   // ddr_cycle
        static constexpr int tRCD = 20;   // ddr_cycle
        static constexpr int tFAW = 34;   // ddr_cycle
        static constexpr int tW2WDR = 10;   // ddr_cycle
        static constexpr int tW2RDR = 10;   // ddr_cycle
        static constexpr int tR2RDR = 14;   // ddr_cycle
        static constexpr int tR2WDR = 8;   // ddr_cycle
};
// convert to mc_cycle, NO NEED TO TOUCH
constexpr int tREFI = (int)((float)TimingConfig::tREFI / 1000 * freq);
constexpr int tZQINTVL = (int)((float)TimingConfig::tZQINTVL * 1000 * freq);
constexpr int tRP = CONVERT_DDR_CYCLE(TimingConfig::tRP);
constexpr int tRFC = (int)((float)TimingConfig::tRFC / 1000 * freq);
constexpr int tZQCS = TimingConfig::tZQCS;
constexpr int tRRD_S = CONVERT_DDR_CYCLE(TimingConfig::tRRD_S);
constexpr int tRRD_L = CONVERT_DDR_CYCLE(TimingConfig::tRRD_L);
constexpr int tCCD_L = CONVERT_DDR_CYCLE(TimingConfig::tCCD_L);
constexpr int tCCD_S = CONVERT_DDR_CYCLE(TimingConfig::tCCD_S);
constexpr int tWTR_L = CONVERT_DDR_CYCLE(TimingConfig::tWTR_L + TimingConfig::WL + (TimingConfig::BL >> 1));
constexpr int tWTR_S = CONVERT_DDR_CYCLE(TimingConfig::tWTR_S + TimingConfig::WL + (TimingConfig::BL >> 1));
constexpr int tRTW = CONVERT_DDR_CYCLE(TimingConfig::tRTW);
constexpr int tWR = CONVERT_DDR_CYCLE(TimingConfig::tWR + TimingConfig::WL + (TimingConfig::BL >> 1));
constexpr int tRTP = CONVERT_DDR_CYCLE(TimingConfig::tRTP);
constexpr int tRAS = CONVERT_DDR_CYCLE(TimingConfig::tRAS);
constexpr int tRCD = CONVERT_DDR_CYCLE(TimingConfig::tRCD);
constexpr int tFAW = CONVERT_DDR_CYCLE(TimingConfig::tFAW) - 1;
constexpr int tW2WDR = CONVERT_DDR_CYCLE(TimingConfig::tW2WDR);
constexpr int tW2RDR = CONVERT_DDR_CYCLE(TimingConfig::tW2RDR);
constexpr int tR2RDR = CONVERT_DDR_CYCLE(TimingConfig::tR2RDR);
constexpr int tR2WDR = CONVERT_DDR_CYCLE(TimingConfig::tR2WDR);

// timestamp params
constexpr int timestamp_x = 1;   // timestamp = timestamp_in_trace * timestamp_x

// common parameters
constexpr int interval = 2;
constexpr int rank_num = 2;   // only support 2 ranks; need to change AddrMap otherwise
constexpr int bg_num = 4;
constexpr int bank_per_bg = 4;
constexpr int all_bank_num = bg_num * bank_per_bg;
// parameters obtained from observing waveforms
constexpr int read_back_delay_from_scg = 30;   // delay from "dfi cmd issue" to "rob get data"
constexpr int read_back_delay_from_cache_hit = 6;   // delay from "cache get cmd" to "rob get data"
constexpr int read_back_delay_from_cache_miss = read_back_delay_from_scg + 5;   // plus delay from "cache get cmd" to "scheduler get cmd"

// size parameters
// AXI2UI
enum class VcMode {
    STATIC, DYNAMIC
};
class Axi2uiConfig {
public:
    static struct QueueSize {
        static constexpr int ar_in = 4;
        static constexpr int ar_out = 8;
        static constexpr int aw_in = 16;
        static constexpr int aw_out = 8;
    } queue_size;
    static constexpr int rob_size = 128;
    static constexpr int vc_num = 1;   // virtual channel
    static constexpr VcMode vc_mode = VcMode::STATIC;
    static constexpr int extra_out_latency = 3;   // latency obtained from observing waveforms
};
// Filter
enum class FilterMode {
    ALL2SCHED, ALL2CACHE, FILTER_ADRBD, FILTER_BIT, FILTER_ADRBD_BIT
};
constexpr FilterMode filter_mode = FilterMode::ALL2CACHE;
constexpr uint64_t adrbdl = 4.08e9;   // libq
constexpr uint64_t adrbdh = 4.14e9;
// constexpr uint64_t adrbdl = 3.7e9;   // lbm
// constexpr uint64_t adrbdh = 4.2e9;
// constexpr uint64_t adrbdl = 3.75e9;   // mcf
// constexpr uint64_t adrbdh = 4.3e9;
constexpr int filter_bit = 22;   // if this bit (count from 1) equals 0, send cmd to cache
constexpr int filter_size = 7;
// AddrMap
constexpr int addr_map_size = 1;
// Cache
class CacheConfig {
public:
    static constexpr bool CACHE_EN = false;
    static constexpr int CMD_QUEUE_DEPTH = 64;
    static constexpr int CACHE_SIZE = 2 * 1024 * 1024;   // B
    // static constexpr int CACHE_SIZE = 256 * 1024;   // B
    static constexpr int CACHELINE_SIZE = 64;  // B
    static constexpr int CACHE_WAYS = 4;
    static constexpr int CACHE_BANKS = 8;
    static constexpr int MSHR_DEPTH = 64;
    static constexpr int WCB_DEPTH = 18;
    // Prefetcher
    static struct PrefetcherConfig {
    public:
        static constexpr bool ENABLE = true;
        static constexpr int GHB_NUM_ENTRIES = 16;   // Global History Buffer for miss addresses
        static constexpr int MAX_STREAMS = 4;   // Maximum number of streams to track
        static constexpr int PREFETCH_THRESHOLD = 4;   // threshold for prefetch
        static constexpr int STRIDE_WIDTH = 8;   // width of the stride in bits
        static constexpr int PREFETCH_NUM = 4;
        // optimal options
        static constexpr bool HIT_INTO_GHB = true;
    } pfc_cfg;
};
// Scheduler
enum class StationOrg {
    SINGLE, PER_BANK, PER_BG
};
enum class SchedulePolicy {
    FCFS, FRFCFS, FRFCFS_P
};
enum class RowPolicy {
    OPEN, CLOSE
};
constexpr StationOrg station_org = StationOrg::PER_BG;
constexpr SchedulePolicy schedule_policy = SchedulePolicy::FRFCFS;
constexpr RowPolicy row_policy = RowPolicy::OPEN;
constexpr int scheduler_queue_size = 2;
constexpr int rstation_size = 64;
// constexpr int wstation_size = 64;
constexpr int wstation_size = 128;
// SCG
enum class ScgArbPolicy {
    RoundRobin, Balanced, FCFS   // TODO: FCFS
};
class ScgConfig {
public:
    static constexpr ScgArbPolicy arb_policy = ScgArbPolicy::RoundRobin;
    static constexpr int hungry_time = 10;   // for Banlanced arb
    static constexpr bool sphase = true;   // single phase
};

// dump info
class DumpConfig {
public:
    // dump to screen
    static constexpr bool DUMP_TRANS_COUNT = true;
    static constexpr bool DUMP_LATENCY = false;
    static constexpr bool DUMP_AS_SCG_TRANS = false;
    static constexpr bool DUMP_DFI_RW_TRANS = false;
    static constexpr bool DUMP_PREFETCH = false;
    // dump to log file
    static constexpr bool DUMP_MODULEWISE_LOG = false;   // dump module-wise trans cycle log
    static constexpr const char* dump_log_dir_path = "./logs";
};

#endif