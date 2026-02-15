/**
 * @file include/StockSeries.hpp
 * @author @betadust
 * @date [2026-02-15]
 */

#pragma once

#include "PricePoint.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <memory>

namespace high_frequency_storage {

    /**
     * @brief 管理单只股票的时间序列数据
     *
     * StockSeries 负责存储和查询一只股票的所有价格数据点。
     * 它提供了数据的添加、范围查询、统计信息获取等核心功能。
     *
     * 设计演进路线：
     * - 阶段一：基于 std::vector 的基础实现
     * - 阶段二：添加差分压缩支持
     * - 阶段三：集成内存池优化
     */
    class StockSeries {
    private:
        // ---------- 核心数据成员 ----------

        std::string stock_code_;              // 股票代码（如 "AAPL"）
        std::vector<PricePoint> metadata_;        // 原始数据点（阶段一）
        bool time_sorted_;                       // 数据是否已排序（用于优化）

        // ---------- 统计信息（缓存加速）----------

        mutable bool stats_dirty_;              // 统计信息是否需要重新计算
        mutable double min_price_;              // 最低价格（缓存）
        mutable double max_price_;              // 最高价格（缓存）
        mutable int64_t min_timestamp_;         // 最早时间戳（缓存）
        mutable int64_t max_timestamp_;         // 最晚时间戳（缓存）

        // ---------- 阶段二扩展（压缩支持）----------

        // 这些成员将在阶段二实现时启用
        // std::vector<int64_t> encoded_timestamps_; // 压缩后的时间戳（差分编码）
        // std::vector<double> encoded_prices_;      // 压缩后的价格
        // bool is_compressed_;                       // 是否已压缩

        // ---------- 阶段三扩展（内存池）----------

        // 这些成员将在阶段三实现时启用
        // std::shared_ptr<class MemoryPool> pool_;  // 内存池句柄
        // std::vector<PricePointHandle> handles_;   // 句柄列表

    public:
        // ---------- 构造函数 / 析构函数 ----------

        // @param code 股票代码        
        explicit StockSeries(const std::string& code);

        ~StockSeries() = default;

        // 禁止拷贝（资源管理考虑，可改为允许如果后续需要）
        StockSeries(const StockSeries&) = delete;
        StockSeries& operator=(const StockSeries&) = delete;

        // 允许移动
        StockSeries(StockSeries&&) = default;
        StockSeries& operator=(StockSeries&&) = default;

        // ---------- 核心数据接口 ----------

        /**
         * @brief 添加一个价格数据点
         * @param timestamp 时间戳
         * @param price 价格
         * @throws std::invalid_argument 如果 timestamp 或 price 无效
         *
         * 注意：调用者应保证 timestamp 大致递增，但本方法不强制。
         * 添加后 time_sorted_ 可能变为 false，查询前会重新排序。
         */
        void addPrice(int64_t timestamp, double price);

        /**
         * @brief 批量添加价格数据点
         * @param points 价格点列表
         *
         */
        void addPrices(const std::vector<PricePoint>& points);

        /**
         * @brief 查询指定时间范围内的价格
         * @param start_time 开始时间（包含）
         * @param end_time 结束时间（包含）
         * @return 价格列表，按时间升序排列
         *
         * 如果 start_time > end_time，返回空列表。
         * 时间复杂度：阶段一为 O(n)，后续优化为 O(log n)
         */
        std::vector<double> queryRange(int64_t start_time, int64_t end_time) const;

        /**
         * @brief 获取指定时间点的价格（最接近的）
         * @param timestamp 目标时间戳
         * @return 价格，如果无数据则返回 0.0
         */
        double getPriceAt(int64_t timestamp) const;

        // ---------- 数据维护接口 ----------

        /**
         * @brief 确保数据按时间戳排序
         *
         * 查询操作会自动调用此方法，通常不需要手动调用。
         */
        void ensureSorted();

        /**
         * @brief 清空所有数据
         */
        void clear();

        /**
         * @brief 预留容量（优化批量添加）
         * @param capacity 预期数据点数量
         */
        void reserve(size_t capacity);

        // ---------- 查询接口（统计信息）----------

        // @brief 获取股票代码        
        const std::string& stockCode() const { return stock_code_; }

        
        // @brief 获取数据点数量
        size_t size() const { return metadata_.size(); }

        // @brief 判断是否为空 
        bool empty() const { return metadata_.empty(); }

        // @brief 获取最低价格         
        double minPrice() const;

        // @brief 获取最高价格
        double maxPrice() const;

        
        // @brief 获取最早时间戳 
        int64_t minTimestamp() const;

        // @brief 获取最晚时间戳        
        int64_t maxTimestamp() const;

       
        // @brief 获取价格范围（最高-最低）  
        double priceRange() const { return maxPrice() - minPrice(); }

        // @brief 获取时间跨度（毫秒）        
        int64_t timeSpan() const { return maxTimestamp() - minTimestamp(); }

        // ---------- 压缩接口（阶段二）----------

        /**
         * @brief 对数据进行差分压缩
         * @return 是否成功压缩
         *
         * 阶段二实现：将原始数据转换为差分编码格式，
         * 可减少内存占用 50% 以上。
         */
         // bool compress();

         /**
          * @brief 解压数据（如果需要）
          */
          // bool decompress();

          /**
           * @brief 检查是否已压缩
           */
           // bool isCompressed() const { return is_compressed_; }

        // ---------- 序列化接口（可选）----------

           
        // @brief 将数据导出为 CSV 格式字符串
        std::string toCSV() const;

        /**
         * @brief 从 CSV 格式字符串加载数据
         * @param csv_data CSV 格式数据
         * @return 成功加载的数据点数量
         */
        size_t fromCSV(const std::string& csv_data);

        // ---------- 调试接口 ----------

        
        // @brief 打印前 n 个数据点（用于调试）
        void printHead(size_t n = 5) const;

        
        // @brief 获取内存占用估算值（字节） 
        size_t memoryUsage() const;

    private:
        // ---------- 内部工具函数 ----------

    
        // @brief 更新统计信息缓存
        void refreshStats() const;

        
        // @brief 检查价格点是否有效
        bool isValidPricePoint(int64_t timestamp, double price) const;

        
        // @brief 二分查找第一个 >= target 的位置
        size_t lowerBound(int64_t target) const;

        
        // @brief 二分查找第一个 > target 的位置
        size_t upperBound(int64_t target) const;
    };

} // namespace high_frequency_storage