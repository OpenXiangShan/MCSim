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

#ifndef SMART_SCAN_PREFETCHER_H
#define SMART_SCAN_PREFETCHER_H

#include "Cache/Prefetcher.h"
#include <vector>
#include <queue>
#include <cstdint>
#include <unordered_map>
#include "config.h"
#include "top/Transaction.h"

class SmartScanPrefetcher : public Prefetcher {
public:
    SmartScanPrefetcher();

    void step() override;
    bool addTrans(const MappedTransaction &t) override;

private:
    // 配置参数
    uint32_t MAX_OBSERVED_ACCESSES;    // 最大观察访问次数（达到后停止更新特征）
    double   DECISION_THRESHOLD_EARLY; // 早期决策阈值（访问次数1-4）
    double   DECISION_THRESHOLD_MID;   // 中期决策阈值（访问次数5）
    double   DECISION_THRESHOLD_LATE;  // 后期决策阈值（访问次数6+）
    uint32_t PAGE_SIZE;                // 页面大小（字节）
    uint32_t CACHE_LINE_SIZE;          // Cache line大小（字节）

    // 特征权重系数（可配置）
    double W_UNIQUE;      // 唯一访问比例权重
    double W_CONTIG;      // 最大连续段比例权重
    double W_SMALL_JUMP;  // 小跳转比例权重
    double W_COMMON_JUMP; // 常见跳转比例权重
    double W_READ;        // 读操作比例权重
    double W_SPAN;        // 访问跨度权重（负向）
    double W_FLIP;        // 方向翻转权重（负向）
    double W_AVG_JUMP;    // 平均跳转权重（负向）

    /**
     * @struct PageFeatures
     * @brief 页面特征结构，存储每个页面的访问特征
     */
    struct PageFeatures {
        std::vector<uint32_t> offsets;          // 按时间顺序记录的cache line偏移
        uint32_t unique_count = 0;              // 唯一访问的cache line数量
        uint32_t max_contiguous = 0;            // 最大连续访问段长度
        uint32_t flip_count = 0;                // 访问方向翻转次数
        uint32_t small_jump_count = 0;          // 小步长跳转次数（距离<=2）
        int32_t  last_offset = -1;              // 上一次访问的偏移
        int32_t  last_direction = 0;            // 上一次访问的方向（正/负）
        bool     predicted = false;             // 是否已触发预取
        double   score = 0.0;                   // 扫描特征得分

        uint32_t read_count = 0;                // 读操作次数
        uint32_t total_count = 0;               // 总访问次数
    };

    // 哈希表存储每个页面的信息，最多存1024条，存满了就随机删
    std::unordered_map<uint64_t, PageFeatures> page_table;
    //用queue或者list？后面可以扩展成更多的替换策略，如果想尝试的删除策略更多的话，这里需要重写包装成一个类方便选择不同策略
    //std::queue<uint64_t> page_table_fifo_index;
    
    // 预取请求队列
    std::vector<uint64_t> prefetch_queue;

    // 核心方法
    uint64_t getPageBase(uint64_t address) const;
    uint32_t getCacheLineOffset(uint64_t address) const;
    
    void updatePageFeatures(PageFeatures& features, uint32_t offset, bool is_read);
    double calculateScanScore(const PageFeatures& features, size_t access_count);
    bool shouldTriggerPrefetch(const PageFeatures& features, size_t access_count);
    
    void generatePrefetchRequests(uint64_t page_base, const PageFeatures& features);
    void doPrefetch();   // 每 cycle 只尝试 issue 一个 prefetch（与 BOP 对齐）
};

#endif // SMART_SCAN_PREFETCHER_H