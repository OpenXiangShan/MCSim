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

#include "Cache/Prefetcher/SmartScanPrefetcher_v2.h"
#include "Cache/Cache.h"
#include <cassert>
#include <iostream>
#include <algorithm>

/**
 * @class SmartScanPrefetcher
 * @brief SCAN页面探测预取算法
 * 
 * 这是一个基于页面特征分析的预取器，通过监控内存访问模式来预测是否需要预取整个页面。
 * 当检测到某个页面的访问模式显示出某些特征时（如连续访问、小步长跳转等），
 * 就预取该页面的所有cache line。
 * 
 * 主要特点：
 * 1. 按照页面内访存的特征进行特征分析
 * 2. 动态计算多个特征指标并加权评分
 * 3. 根据访问次数动态调整决策阈值
 * 4. 支持配置化参数，便于调优
 */

SmartScanPrefetcher::SmartScanPrefetcher()
{
    // 从 CacheConfig 读取参数
    MAX_OBSERVED_ACCESSES   = CacheConfig::pfc_cfg.SSCAN_MAX_OBSERVED_ACCESSES;     // 最大观察访问次数（达到后停止学习）
    DECISION_THRESHOLD_EARLY = CacheConfig::pfc_cfg.SSCAN_DECISION_THRESHOLD_EARLY; // 早期决策阈值（访问次数少时）
    DECISION_THRESHOLD_MID   = CacheConfig::pfc_cfg.SSCAN_DECISION_THRESHOLD_MID;   // 中期决策阈值
    DECISION_THRESHOLD_LATE  = CacheConfig::pfc_cfg.SSCAN_DECISION_THRESHOLD_LATE;  // 后期决策阈值（访问次数多时）
    PAGE_SIZE       = CacheConfig::pfc_cfg.SSCAN_PAGE_SIZE;                         // 页面大小（字节）
    CACHE_LINE_SIZE = CacheConfig::pfc_cfg.SSCAN_CACHELINE_SIZE;                    // Cache line大小（字节）

    //算分权重
    W_UNIQUE      = CacheConfig::pfc_cfg.SSCAN_W_UNIQUE;
    W_CONTIG      = CacheConfig::pfc_cfg.SSCAN_W_CONTIG;
    W_SMALL_JUMP  = CacheConfig::pfc_cfg.SSCAN_W_SMALL_JUMP;
    W_COMMON_JUMP = CacheConfig::pfc_cfg.SSCAN_W_COMMON_JUMP;
    W_READ        = CacheConfig::pfc_cfg.SSCAN_W_READ;
    W_SPAN        = CacheConfig::pfc_cfg.SSCAN_W_SPAN;
    W_FLIP        = CacheConfig::pfc_cfg.SSCAN_W_FLIP;
    W_AVG_JUMP    = CacheConfig::pfc_cfg.SSCAN_W_AVG_JUMP;

    page_table.reserve(1024);
}

/**
 * @brief 主处理函数，每个时钟周期调用一次
 * 
 * 功能：
 * 1. 尝试发送一个预取请求（doPrefetch）
 * 2. 检查页面表大小，如果超过1024条则删除随机删除256条，受限于哈希表的特性，目前先随机删除，FIFO和LRU策略需要其他容器
 */

void SmartScanPrefetcher::step()
{
    doPrefetch();   // 每 cycle 只 issue 一个 prefetch

    // 
    if (page_table.size() > 1024) {
        auto start = page_table.begin();
        auto end = page_table.begin();
        std::advance(end, std::min<size_t>(256, page_table.size()));
        page_table.erase(start, end);
    }
}

/**
 * @brief 尝试发送预取请求
 * 
 * 每周期只尝试发送队列中的第一个预取请求。
 * 如果缓存接受该请求（addPrefetch返回true），则从队列中移除；
 * 如果缓存拒绝（队列满），则保留在队列中，下一周期重试。
 * 
 */
// 每 cycle 只尝试发送一个 prefetch
void SmartScanPrefetcher::doPrefetch()
{
    if (!prefetch_queue.empty()) {
        uint64_t addr = prefetch_queue.front();
        if (cache->addPrefetch(addr)) {
            prefetch_queue.erase(prefetch_queue.begin());
        }
        // 如果 cache 当前无法接受（队列满等），下一 cycle 继续尝试
    }
}

/**
 * @brief 处理内存访问事务，更新页面特征，由流水线上一级调用
 * @param t 内存访问事务
 * @return 是否成功处理（总是返回true，除非地址为0）
 * 
 * 核心逻辑：
 * 1. 从地址计算页面基址和cache line偏移
 * 2. 获取或创建该页面的特征记录（page_table）
 * 3. 如果该页面尚未触发预取且观察次数未达到上限，则更新特征
 * 4. 检查是否应该触发预取，如果应该则标记为已预测并生成预取请求
 */

bool SmartScanPrefetcher::addTrans(const MappedTransaction &t)
{
    if (t.addr == 0) return false;

    uint64_t page_base = getPageBase(t.addr);
    uint32_t offset    = getCacheLineOffset(t.addr);
    bool is_read = t.is_read;                    

    auto& features = page_table[page_base];

    if (!features.predicted && features.total_count < MAX_OBSERVED_ACCESSES) {
        updatePageFeatures(features, offset, is_read); 

        if (shouldTriggerPrefetch(features, features.total_count)) {
            features.predicted = true;
            generatePrefetchRequests(page_base, features);
        }
    }

    return true;
}

/**
 * @brief 计算页面基址（页面对齐的起始地址）
 * @param address 内存地址
 * @return 页面基址
 * 
 * 通过位运算清除低PAGE_SIZE位，得到页面对齐的基址。
 * 例如：PAGE_SIZE=4096(0x1000)，则清除低12位。
 */
uint64_t SmartScanPrefetcher::getPageBase(uint64_t address) const
{
    return address & ~(static_cast<uint64_t>(PAGE_SIZE) - 1);
}

/**
 * @brief 计算cache line偏移（在页面内的偏移，以cache line为单位）
 * @param address 内存地址
 * @return cache line偏移（相较于基地址，是在这个页里面的第几个cache line中）
 * 
 * 先计算页面内字节偏移，然后除以cache line大小（64B）得到line偏移。
 */
uint32_t SmartScanPrefetcher::getCacheLineOffset(uint64_t address) const
{
    uint32_t offset_in_page = address & (PAGE_SIZE - 1);
    return offset_in_page / CACHE_LINE_SIZE;
}

/**
 * @brief 更新页面特征
 * @param features 要更新的页面特征
 * @param offset 当前访问的cache line偏移
 * @param is_read 是否为读操作
 * 
 * 更新内容：
 * 1. 记录访问偏移、总访问次数、读操作次数
 * 2. 计算唯一访问数（去重后的访问偏移数）后面如果想使用被去重排序后的offsets，可以使用unique_offsets
 * 3. 计算最大连续访问段长度，在去重排序后的offsets上计算的
 * 4. 更新方向翻转次数和小步长跳转次数，暂定<=2的偏移为小步长跳转
 * 5. 计算该页面的扫描特征得分
 */
void SmartScanPrefetcher::updatePageFeatures(PageFeatures& features, uint32_t offset, bool is_read)
{
    features.offsets.push_back(offset);

    features.total_count++;                     

    if (is_read) {
        features.read_count++;                 
    }

    // 计算唯一访问数
    {
        std::vector<uint32_t> unique_offsets = features.offsets;
        std::sort(unique_offsets.begin(), unique_offsets.end());
        auto last = std::unique(unique_offsets.begin(), unique_offsets.end());
        features.unique_count = std::distance(unique_offsets.begin(), last);
    }

    // 计算最大连续段（在 unique offsets 上计算）
    {
        std::vector<uint32_t> sorted = features.offsets;
        std::sort(sorted.begin(), sorted.end());
        auto last = std::unique(sorted.begin(), sorted.end());
        sorted.resize(std::distance(sorted.begin(), last));

        uint32_t current = 1;
        features.max_contiguous = 1;
        for (size_t i = 1; i < sorted.size(); ++i) {
            if (sorted[i] == sorted[i-1] + 1) {
                ++current;
                features.max_contiguous = std::max(features.max_contiguous, current);
            } else {
                current = 1;
            }
        }
    }

    // 更新方向翻转和小步长
    if (features.last_offset != -1) {
        int32_t curr_dir = static_cast<int32_t>(offset) - features.last_offset;
        if (features.last_direction != 0 &&
            ((curr_dir > 0 && features.last_direction < 0) ||
             (curr_dir < 0 && features.last_direction > 0))) {
            features.flip_count++;
        }
        features.last_direction = (curr_dir != 0) ? curr_dir : features.last_direction;
    }

    if (features.last_offset != -1) {
        int32_t jump = std::abs(static_cast<int32_t>(offset) - features.last_offset);
        if (jump <= 2) {
            features.small_jump_count++;
        }
    }

    features.last_offset = offset;

    // 计算评分
    features.score = calculateScanScore(features, features.total_count);
}

/**
 * @brief 计算页面扫描特征得分
 * @param features 页面特征
 * @param access_count 访问次数
 * @return 扫描特征得分（越高表示越像SCAN）
 * 
 * 评分公式包含8个维度，每个维度都有对应的权重：
 * 1. unique_ratio: 唯一访问比例（高分好）
 * 2. contig_ratio: 最大连续段比例，被视作连续段的间隔默认是1，就是以1递增的数列（高分好）
 * 3. small_jump_ratio: 小跳转比例（高分好）
 * 4. common_jump_ratio: 常见跳转比例（高分好）
 * 5. read_ratio: 读操作比例（高分好）
 * 6. span_norm: 访问跨度归一化值（低分好，用负号）
 * 7. flips_norm: 方向翻转比例（低分好，用负号）
 * 8. avg_jump_norm: 平均跳转距离归一化值（低分好，用负号）
 * 
 * 最终得分 = (权重 × 特征值)求和，得分越高表示越符合SCAN。
 */
double SmartScanPrefetcher::calculateScanScore(const PageFeatures& features, size_t access_count)
{
    if (access_count == 0) return 0.0;

    const auto& offsets = features.offsets;
    size_t k = access_count;

    // unique_count、span、best_contig 计算
    std::vector<uint32_t> unique_offsets = offsets;
    std::sort(unique_offsets.begin(), unique_offsets.end());
    auto last_it = std::unique(unique_offsets.begin(), unique_offsets.end());
    unique_offsets.resize(std::distance(unique_offsets.begin(), last_it));

    uint32_t unique_count = unique_offsets.size();
    uint32_t span = unique_offsets.empty() ? 0 : (unique_offsets.back() - unique_offsets.front() + 1);
    uint32_t best_contig = 1;

    if (!unique_offsets.empty()) {
        uint32_t current = 1;
        best_contig = 1;
        for (size_t i = 1; i < unique_offsets.size(); ++i) {
            if (unique_offsets[i] == unique_offsets[i-1] + 1) {
                ++current;
                best_contig = std::max(best_contig, current);
            } else {
                current = 1;
            }
        }
    }

    // deltas 计算，这里使用的是没有排序和去重的offsets数组
    std::vector<int32_t> deltas;
    if (k > 1) {
        deltas.reserve(k - 1);
        for (size_t i = 1; i < k; ++i) {
            deltas.push_back(static_cast<int32_t>(offsets[i]) - static_cast<int32_t>(offsets[i-1]));
        }
    }

    double small_jump_ratio = 0.0;
    double common_jump_ratio = 0.0;
    uint32_t flips = 0;
    double avg_jump = 0.0;

    if (!deltas.empty()) {
        std::vector<uint32_t> abs_deltas(deltas.size());
        for (size_t i = 0; i < deltas.size(); ++i) {
            abs_deltas[i] = std::abs(deltas[i]);
        }

        uint32_t num_small = 0;
        for (auto d : abs_deltas) if (d <= 2) ++num_small;
        small_jump_ratio = static_cast<double>(num_small) / deltas.size();

        if (!abs_deltas.empty()) {
            uint32_t max_freq = 0;
            for (auto d : abs_deltas) {
                uint32_t freq = 0;
                for (auto dd : abs_deltas) if (dd == d) ++freq;
                max_freq = std::max(max_freq, freq);
            }
            common_jump_ratio = static_cast<double>(max_freq) / deltas.size();
        }

        for (size_t i = 1; i < deltas.size(); ++i) {
            if (deltas[i] * deltas[i-1] < 0) ++flips;
        }

        double sum_abs = 0.0;
        for (auto d : abs_deltas) sum_abs += d;
        avg_jump = sum_abs / deltas.size();
    }
    //算读写比例
    double read_ratio = (features.total_count > 0) 
                        ? static_cast<double>(features.read_count) / features.total_count 
                        : 1.0;

    double span_norm     = static_cast<double>(span) / (static_cast<double>(PAGE_SIZE) / CACHE_LINE_SIZE);
    double contig_ratio  = static_cast<double>(best_contig) / k;
    double unique_ratio  = static_cast<double>(unique_count) / k;
    double flips_norm    = static_cast<double>(flips) / std::max(1.0, static_cast<double>(k) - 2.0);
    double avg_jump_norm = std::min(avg_jump / 16.0, 2.0);

    //加权重算分
    return (
        W_UNIQUE      * unique_ratio +
        W_CONTIG      * contig_ratio +
        W_SMALL_JUMP  * small_jump_ratio +
        W_COMMON_JUMP * common_jump_ratio +
        W_READ        * read_ratio -
        W_SPAN        * span_norm -
        W_FLIP        * flips_norm -
        W_AVG_JUMP    * avg_jump_norm
    );
}

/**
 * @brief 判断是否应该触发预取
 * @param features 页面特征
 * @param access_count 访问次数
 * @return true表示应该触发预取
 * 
 * 触发条件（需要同时满足）：
 * 1. 访问次数>=4（避免过早预取）
 * 2. 唯一访问数足够多（至少4次或访问次数-1）
 * 3. 访问跨度不能太大（<=32个cache line）
 * 4. 特征得分超过动态阈值（根据访问次数调整）
 * 
 * 动态阈值策略：
 * - 访问次数<=4：使用早期阈值（较严格）
 * - 访问次数==5：使用中期阈值
 * - 访问次数>5：使用后期阈值（较宽松）
 * 
 * 这种设计避免了在访问模式不明确时过早预取，也防止了在访问模式明确时错过预取机会。
 */
bool SmartScanPrefetcher::shouldTriggerPrefetch(const PageFeatures& features, size_t access_count)
{
    // 动态决策阈值，对应should_predict_full
    if (access_count < 4) {
        return false;
    }

    if (features.unique_count < std::max(4u, static_cast<uint32_t>(access_count) - 1)) {
        return false;
    }

    if (!features.offsets.empty()) {
        auto minmax = std::minmax_element(features.offsets.begin(), features.offsets.end());
        uint32_t span = *minmax.second - *minmax.first + 1;
        if (span > 32) {
            return false;
        }
    }

    // 所需得分随访问次数衰减，这里没在config里配置
    double threshold;
    if (access_count <= 4) {
        threshold = DECISION_THRESHOLD_EARLY;
    } else if (access_count == 5) {
        threshold = DECISION_THRESHOLD_MID;
    } else {
        threshold = DECISION_THRESHOLD_LATE;
    }

    return features.score >= threshold;
}

/**
 * @brief 生成预取请求
 * @param page_base 页面基址
 * @param features 页面特征（虽然未使用，但保留接口）
 * 
 * 一旦决定预取某个页面，就预取该页面的所有cache line。
 * 
 * 预取请求添加到队列中，由doPrefetch()方法按周期发送。
 */
void SmartScanPrefetcher::generatePrefetchRequests(uint64_t page_base, const PageFeatures& features)
{
    uint32_t lines_per_page = PAGE_SIZE / CACHE_LINE_SIZE;

    for (uint32_t i = 0; i < lines_per_page; ++i) {
        uint64_t addr = page_base + i * CACHE_LINE_SIZE;
        prefetch_queue.push_back(addr);
    }
}