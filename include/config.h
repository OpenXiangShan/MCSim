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

// Review configuration for the exact 2026-09-24 Baiyang wrapper under
// verilator_baiyang environment.
// Verified against Baiyang/build/BY_mc_top.sv used by that Verilator build.
// APB addresses are byte addresses. Use emitted RTL slices, not guessed
// Bundle packing: e.g. tRP=tmchk0[5:0], tRRDS=tmchk0[31:26].
//
// This aligns representable values/capacities; it is NOT a claim of cycle
// equivalence.
// Remaining source-level gaps include refresh timer/FSM behavior, AXI-ID
// remapping, wrapper CDC/route queues, and calibrated read-return delays.
// Rebuild libmcsim AND relink emu for changes to affect the static executable.

#ifndef CONFIG_H
#define CONFIG_H
#include <cstdint>
#define CONVERT_DDR_CYCLE(a) (((a + 1) >> 1) - 1)

// timing parameters
// constexpr int freq = 600;   // MHz
// class TimingConfig {
//     public:
//         static constexpr int tREFI = 7800;   // ns
//         // static constexpr int tREFI = 999999999;   // no refresh
//         static constexpr int tZQINTVL = 128;   // ms
//         static constexpr int tRP = 22;   // ddr_cycle
//         static constexpr int tRFC = 650;   // ns
//         static constexpr int tZQCS = 128;   // mc_cycle
//         static constexpr int tRRD_S = 4;   // ddr_cycle
//         static constexpr int tRRD_L = 8;   // ddr_cycle
//         static constexpr int tCCD_L = 8;   // ddr_cycle
//         static constexpr int tCCD_S= 4;   // ddr_cycle
//         static constexpr int tWTR_L = 32;   // ddr_cycle
//         static constexpr int tWTR_S = 24;   // ddr_cycle
//         static constexpr int BL = 8;   // ddr_cycle
//         static constexpr int RL = 28;   // ddr_cycle (RL = AL + CL + PL, AL = PL = 0, CL = 10)
//         static constexpr int WL = 12;   // ddr_cycle
//         static constexpr int tRTW = 10;   // ddr_cycle
//         static constexpr int tWR = 24;   // ddr_cycle
//         static constexpr int tRTP = 10;   // ddr_cycle
//         static constexpr int tRAS = 54;   // ddr_cycle
//         static constexpr int tRCD = 20;   // ddr_cycle
//         static constexpr int tFAW = 34;   // ddr_cycle
//         static constexpr int tW2WDR = 10;   // ddr_cycle
//         static constexpr int tW2RDR = 10;   // ddr_cycle
//         static constexpr int tR2RDR = 14;   // ddr_cycle
//         static constexpr int tR2WDR = 8;   // ddr_cycle
// };
// Raw timing values from the wrapper APB writes, plus unwritten reset values.
constexpr int freq = 800;   // MHz
class TimingConfig {
    public:
        static constexpr int tREFI = 7800;   // ns
        // static constexpr int tREFI = 999999999;   // no refresh
        static constexpr int tZQINTVL = 128;   // ms: ref0 reset; wrapper does not write byte 0x00c
        static constexpr int tRP = 22;   // ddr_cycle
        static constexpr int tRFC = 350;   // ns: APB ref2[19:8]; 350*800/1000 = 280 MC cycles
        static constexpr int tZQCS = 128;   // mc_cycle
        static constexpr int tRRD_S = 4;   // ddr_cycle
        static constexpr int tRRD_L = 8;   // ddr_cycle
        static constexpr int tCCD_L = 8;   // ddr_cycle
        static constexpr int tCCD_S= 4;   // ddr_cycle
        static constexpr int tWTR_L = 12;   // ddr_cycle
        static constexpr int tWTR_S = 4;   // ddr_cycle
        static constexpr int BL = 8;   // ddr_cycle
        static constexpr int RL = 20;   // ddr_cycle
        static constexpr int WL = 16;    // ddr_cycle
        static constexpr int tRTW = 10;   // ddr_cycle
        static constexpr int tWR = 24;   // ddr_cycle
        static constexpr int tRTP = 12;   // ddr_cycle
        static constexpr int tRAS = 52;   // ddr_cycle
        static constexpr int tRCD = 20;   // ddr_cycle
        static constexpr int tFAW = 34;   // ddr_cycle
        static constexpr int tW2WDR = 10;   // ddr_cycle
        static constexpr int tW2RDR = 10;   // ddr_cycle
        static constexpr int tR2RDR = 12;   // ddr_cycle
        static constexpr int tR2WDR = 10;   // ddr_cycle
};
// Existing MCSim conversion conventions. Counter preload and elapsed time
// are distinct: Baiyang tRFC preload is 279; the duration is 280 MC cycles.
// Postponed tREFI arithmetic/initial phase still differs in BackGroundHold.cc.
// Keep raw physical values here; do not distort tRFC/tREFI to mask FSM gaps.
constexpr int tREFI = (int)((float)TimingConfig::tREFI / 1000 * freq) - 1;   // minus 1 to temporarily align with Baiyang-v0.94
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
constexpr int RL = CONVERT_DDR_CYCLE(TimingConfig::RL) + 1;
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

// trace format param
constexpr bool NEW_TRACE = true;   // old_trace: tm, r/w, addr; new_trace: id, dep, old_trace
constexpr bool USE_DEPENDENCY = false;
constexpr int average_axi_latency = 70;   // NOTICE: only used before HMTT can grep r/b channel

// timestamp params
constexpr float cpu_cycle_x = 1;   // mul this value to get real cpu cycle (upstream not fully processed)
constexpr float cpu_mc_freq_rate = 4;   // FPGA: cpu 50M / synopsys MC 12.5M
// constexpr float cpu_mc_freq_rate = 3.75;   // palladium: 3GB / 800M
constexpr float timestamp_x = 1;   // timestamp = timestamp_in_trace * timestamp_x
// constexpr float timestamp_x = cpu_cycle_x / cpu_mc_freq_rate;

// common parameters
constexpr int interval = 2;
constexpr int rank_num = 2;   // only support 2 ranks; need to change AddrMap otherwise
constexpr int bg_num = 4;
constexpr int bank_per_bg = 4;
constexpr int all_bank_num = rank_num * bg_num * bank_per_bg;

// Dual Channel Configuration
enum class ChannelMapMode {
    XOR,
    BIT_SELECT
};
constexpr int NUM_CHANNELS = 2;   // Number of memory channels
constexpr ChannelMapMode channel_map_mode = ChannelMapMode::BIT_SELECT;
// Byte-address bits [5:0] select a byte in the 64-byte transaction.
constexpr int CHANNEL_SELECT_BIT = 6;
constexpr int CHANNEL_XOR_START_BIT = 6;   // Starting bit for XOR channel mapping
constexpr int CHANNEL_XOR_STEP = 3;   // Step between bits (6, 9, 12, ..., 63)
constexpr int CHANNEL_XOR_END_BIT = 63;   // Ending bit for XOR channel mapping

// Existing empirical latency constants, NOT validated for this wrapper/PHY.
// APB dfi0 uses trddataEn=10 and the C++ PHY adds 10 cycles after rddata_en.
// Updating RL alone does not prove the return path matches.
// parameters obtained from observing waveforms
constexpr int scg_cmd_to_dfi_latch_delay = 1;   // TimingArb command win to DFIPhaseCtrl command issue latch
constexpr int scg_second_rddata_beat_delay = 1;   // SCG data is complete after the second DFI rddata beat
constexpr int scg_cas_issue_to_dfi_data_delay = RL + 4 + scg_cmd_to_dfi_latch_delay + scg_second_rddata_beat_delay;
constexpr int scg_dfi_data_to_rob_delay = 2;   // complete DFI data -> SCG RdDataBuffer -> ROB visibility
constexpr int read_back_delay_from_cache_hit = 2;   // delay from cache hit data returning to AXI2UI to ROB visibility
constexpr int read_back_delay_from_cache_miss = 3;   // delay from Cache receiving SCG data to cache miss readback candidate ready

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
    // MCSim passes this capacity to EACH VC, not to a shared total ROB.
    // Baiyang: RobEntryNum=128 / VirtualChannelNum=16 = 8 entries per VC.
    static constexpr int rob_size = 8;
    static constexpr int vc_num = 16;  // virtual channel
    static constexpr VcMode vc_mode = VcMode::STATIC;
    static constexpr int rob_to_axi_rlast_delay = 4;   // ROB-visible data to AXI RLAST latency
};

// Filter
enum class FilterMode {
    ALL2SCHED, ALL2CACHE, FILTER_ADRBD, FILTER_BIT, FILTER_ADRBD_BIT
};
// Explicitly matches APB mode 0. With CACHE_EN=false the old ALL2CACHE
// already bypassed cache in Filter::do_filter; this enum change is redundant.
constexpr FilterMode filter_mode = FilterMode::ALL2SCHED;
constexpr uint64_t adrbdl = 4.08e9;   // libq
constexpr uint64_t adrbdh = 4.14e9;
// constexpr uint64_t adrbdl = 3.7e9;   // lbm
// constexpr uint64_t adrbdh = 4.2e9;
// constexpr uint64_t adrbdl = 3.75e9;   // mcf
// constexpr uint64_t adrbdh = 4.3e9;
constexpr int filter_bit = 22;   // if this bit (count from 1) equals 0, send cmd to cache
constexpr int filter_size = 8;   // filter_top instantiates read/write filter_fifo(..., 8)

// AddrMap
// Queue capacity, NOT an APB mode selector. Current linear_to_mapped already
// matches mode 3 after removing channel bit 6. Active addTransComb bypasses
// these queues; changing this value does not select an address mapping.
constexpr int addr_map_size = 1;

// Cache
class CacheConfig {
public:
    static constexpr bool CACHE_EN = false;
    static constexpr int CMD_QUEUE_DEPTH = 64;
    // static constexpr unsigned long CACHE_SIZE = 2UL << 30;   // B
    static constexpr unsigned long CACHE_SIZE = 256 * 1024;   // B
    static constexpr int CACHELINE_SIZE = 64;  // B
    static constexpr int CACHE_WAYS = 4;
    static constexpr int CACHE_BANKS = 8;
    static constexpr int MSHR_DEPTH = 64;
    static constexpr int WCB_DEPTH = 18;
    static constexpr bool LRU_EVICT = true;
    // Prefetcher
    enum class PrefetcherType {
        NONE, STREAM, SPATIAL, BOP, NEXT_N_LINE, SPP, SSCAN
    };
    static struct PrefetcherConfig {
    public:
        static constexpr PrefetcherType type = PrefetcherType::SSCAN;
        static constexpr bool ENABLE = (type != PrefetcherType::NONE);
        // ==================== SSCAN (Smart Scan Prefetcher) V2 ====================    
        static constexpr int SSCAN_MAX_OBSERVED_ACCESSES = 8;          // Decision window size    
        static constexpr double SSCAN_DECISION_THRESHOLD_EARLY = 3.5;  // 前4次访问阈值    
        static constexpr double SSCAN_DECISION_THRESHOLD_MID   = 3.0;  // 第5次访问阈值    
        static constexpr double SSCAN_DECISION_THRESHOLD_LATE  = 2.5;  // 第6~8次访问阈值    
        static constexpr int SSCAN_PAGE_SIZE = 4096;    
        static constexpr int SSCAN_CACHELINE_SIZE = CACHELINE_SIZE;       // 可配置的特征权重（与 Python predict_improved_v2.py 完全对应）    
        static constexpr double SSCAN_W_UNIQUE       = 1.8;   // unique_ratio    
        static constexpr double SSCAN_W_CONTIG       = 1.6;   // contig_ratio    
        static constexpr double SSCAN_W_SMALL_JUMP   = 1.2;   // small_jump_ratio    
        static constexpr double SSCAN_W_COMMON_JUMP  = 0.8;   // common_jump_ratio    
        static constexpr double SSCAN_W_READ         = 1.2;   // read_ratio    
        static constexpr double SSCAN_W_SPAN         = 1.4;   // span_norm 惩罚系数    
        static constexpr double SSCAN_W_FLIP         = 0.8;   // flips_norm 惩罚系数
        static constexpr double SSCAN_W_AVG_JUMP     = 1.0;   // avg_jump_norm 惩罚系数
        
        // config for stream prefetcher
        static constexpr int GHB_NUM_ENTRIES = 16;   // Global History Buffer for miss addresses
        static constexpr int MAX_STREAMS = 4;   // Maximum number of streams to track
        static constexpr int PREFETCH_THRESHOLD = 4;   // threshold for prefetch
        static constexpr int STRIDE_WIDTH = 8;   // width of the stride in bits
        static constexpr int PREFETCH_DEGREE = 1;
        static constexpr bool FST_HIT_INTO_GHB = true;   // optimal options
        // config for spatial prefetcher
        static constexpr int PREFETCH_GRANULARITY = 8;   // how many cachelines per prefetch
        // config for BOP (Best Offset Prefetcher)
        static constexpr int BOP_MAX_OFFSET = 16;         // Maximum offset value
        static constexpr int BOP_MAX_SCORE = 15;          // Max score to finalize learning phase
        static constexpr int BOP_MAX_ROUNDS = 50;         // Max rounds before forcing offset update
        static constexpr int BOP_BAD_SCORE = 1;           // Score below which offset is considered bad
        static constexpr int BOP_RR_INDEX_BITS = 8;       // RR table index bits
        static constexpr int BOP_PAGE_OFFSET_BITS = 7;    // Page offset bits for cross-page check
        // config for NextNLine prefetcher
        static constexpr uint64_t NEXT_N_LINE_BIT_VECTOR = 0x1;  // Prefetch pattern
        // config for SPP (Signature Path Prefetcher)
        static constexpr int SPP_PAGE_OFFSET_BITS = 7;    // Cache lines per page
        static constexpr int SPP_ST_SETS = 64;            // Signature Table sets
        static constexpr int SPP_ST_WAYS = 4;             // Signature Table ways
        static constexpr int SPP_SIGNATURE_BITS = 9;      // Signature width
        static constexpr int SPP_N_DELTAS = 4;            // Deltas per Pattern Table entry
        static constexpr int SPP_COUNTER_BITS = 4;        // Saturating counter bits
        static constexpr int SPP_CFD_BITS = 7;            // Confidence fixed-point bits
        static constexpr int SPP_PREF_THRESHOLD = 32;     // Prefetch threshold
        static constexpr int SPP_MIN_ALPHA = 16;          // Minimum alpha
        static constexpr int SPP_PFT_ENTRIES = 1024;      // Prefetch Filter Table entries
        static constexpr int SPP_PFT_TAG_BITS = 6;        // PFT tag bits
        static constexpr int SPP_MAX_LOOKAHEAD = 8;       // Maximum lookahead depth per miss
        static constexpr int SPP_MISS_QUEUE_DEPTH = 16;   // Miss queue depth
        static constexpr int SPP_PREF_QUEUE_DEPTH = 32;   // Prefetch queue depth
    } pfc_cfg;
};

// Scheduler
enum class StationOrg {
    SINGLE, PER_BANK, PER_BG
};
enum class SchedulePolicy {
    FCFS, FRFCFS, FRFCFS_P
};
class SchedulerConfig {
public:
    static constexpr StationOrg station_org = StationOrg::PER_BG;
    static constexpr SchedulePolicy schedule_policy = SchedulePolicy::FRFCFS;
    static constexpr bool switch_to_w_when_r_empty = false;   // matches RWSwitch: rHold>27 && wHigh
    static constexpr int scheduler_queue_size = 2;
    // Per rank+bg station depth, matching Baiyang Rd/WrSchedulerQueueDepth.
    // Consumers set read watermarks to 3 / (rstation_size-2) = 3/14,
    // and write watermarks to 2 / wstation_size = 2/32, matching Baiyang.
    static constexpr int rstation_size = 16;
    static constexpr int wstation_size = 32;
};

// SCG
enum class RowPolicy {
    OPEN, CLOSE, DYNAMIC
};   // OPEN: always try to keep rows open; CLOSE: close row after several cycles
enum class RefreshPolicy {
    AUTO, POSTPONED, SPECULATIVE   // TODO: SPECULATIVE
};
enum class ScgArbPolicy {
    RoundRobin, Balanced, FCFS   // TODO: FCFS
};
class ScgConfig {
public:
    static constexpr RowPolicy row_policy = RowPolicy::OPEN;
    static constexpr int close_row_time = 35;
    // Matches scgMode=1 at policy level only; see refresh tests/report.
    static constexpr RefreshPolicy ref_policy = RefreshPolicy::POSTPONED;
    static constexpr ScgArbPolicy arb_policy = ScgArbPolicy::RoundRobin;
    static constexpr int hungry_time = 10;   // for Banlanced arb
    static constexpr bool sphase = true;   // FlowCtrl/DFIPhaseCtrl issue on phase 0 only
    static constexpr int read_back_queue_size = 16;   // match Baiyang v0.94 RdDataBuffer token/data FIFO depth
};

// dump info
class DumpConfig {
public:
    // dump to screen
    static constexpr bool DUMP_TRANS_COUNT = true;
    static constexpr bool DUMP_LATENCY = true;
    static constexpr bool DUMP_AS_SCG_TRANS = false;
    static constexpr bool DUMP_DFI_RW_TRANS = false;
    static constexpr bool DUMP_PREFETCH = false;
    // dump to log file
    static constexpr bool DUMP_MODULEWISE_LOG = false;   // dump module-wise trans cycle log
    static constexpr const char* dump_log_dir_path = "./logs";
};

// tmp test config
constexpr bool TMP = false;

#endif
