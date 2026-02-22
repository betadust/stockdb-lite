/**
 * @file include/StockSeries.hpp
 * @author @betadust
 * @date [2026-02-21]
 */

#pragma once

#include "PricePoint.hpp"
#include "Compressor.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <shared_mutex>

namespace high_frequency_storage {

    /**
     * @brief 管理单只股票的时间序列数据
     *
     * StockSeries 负责存储和查询一只股票的所有价格数据点。
     * 它提供了数据的添加、范围查询、统计信息获取等核心功能。
     *
     * 设计演进路线：
     * - 阶段一：基于 std::vector 的基础实现 √
     * - 阶段二：添加差分压缩支持            now
     * - 阶段三：集成内存池优化              ×
     */
    
    
    /**
     * @brief 压缩段信息
     */
    struct CompressedSegment {
        CompressedBlock block;              // 压缩块元信息
        std::vector<uint8_t> data;          // 压缩后的数据
        int64_t min_timestamp;               // 段内最小时间戳
        int64_t max_timestamp;               // 段内最大时间戳
        size_t memoryUsage() const {
            return sizeof(*this) + data.capacity() + block.timestamp_bytes + block.price_bytes;
        }
    };

    class StockSeries {
    private:
        // ---------- 核心数据成员 ----------
        
        std::string stock_code_;              // 股票代码（如 "AAPL"）
        
        // 未压缩元数据设计
        //std::vector<PricePoint> metadata_;    // 原始数据点（阶段一）timestamp始终有序

        // 双缓冲区设计
        std::vector<PricePoint> active_buffer_;           // 活跃缓冲区（未压缩）
        std::vector<CompressedSegment> compressed_segments_; // 压缩段列表
                     
        // 统计信息缓存
        mutable double min_price_;              // 最低价格（缓存）
        mutable double max_price_;              // 最高价格（缓存）
        mutable int64_t min_timestamp_;         // 最早时间戳（缓存）
        mutable int64_t max_timestamp_;         // 最晚时间戳（缓存）
		size_t total_points_;                      // 数据点总数（包括压缩和未压缩）

        // 缓冲区配置
        static constexpr size_t ACTIVE_BUFFER_LIMIT = 10000;  // 1万点触发压缩
        static constexpr size_t MAX_SEGMENT_SIZE = 50000;      // 最大段大小（压缩前点数）

        // 并发控制（如果需要）
        mutable std::shared_mutex rw_mutex_;


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

        // --添加数据时，由调用者保证写锁占用！
		// --查询数据时，由调用者保证读锁占用！
		// --查询数据时，由被调用者写锁占用（如果需要排序或更新统计信息）！

        /**
         * @brief 添加一个价格数据点
         * @param timestamp 时间戳
         * @param price 价格
         * @throws std::invalid_argument 如果 timestamp 或 price 无效
         *
         * 注意：调用者应保证 timestamp 不严格递增！
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

        // @brief 获取所有数据（未压缩）
		std::vector<PricePoint> get_all_data() const;

        // ---------- 数据维护接口 ----------
        
        /**
         * @brief 强制压缩活跃缓冲区
         */
        void flush();
        
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
        size_t size() const {
            std::shared_lock lock(rw_mutex_);
            return total_points_;
        }
        
        // @brief 判断是否为空 
        bool empty() const {
            std::shared_lock lock(rw_mutex_);
            return active_buffer_.empty() && compressed_segments_.empty();
        }

        // @brief 获取最低价格         
        double minPrice() const {
            std::shared_lock lock(rw_mutex_);
            return min_price_;
        }

        // @brief 获取最高价格
        double maxPrice() const {
            std::shared_lock lock(rw_mutex_);
            return max_price_;
        }

        // @brief 获取最早时间戳 
        int64_t minTimestamp() const {
            std::shared_lock lock(rw_mutex_);
            return min_timestamp_;
        }

        // @brief 获取最晚时间戳        
        int64_t maxTimestamp() const {
            std::shared_lock lock(rw_mutex_);
            return max_timestamp_;
        }

        // @brief 获取价格范围（最高-最低）  
        double priceRange() const { return maxPrice() - minPrice(); }

        // @brief 获取时间跨度（毫秒）        
        int64_t timeSpan() const { return maxTimestamp() - minTimestamp(); }

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


        // ---------- 内部数据结构访问（谨慎使用）----------

        const std::vector<PricePoint> getActiveBuffer() const { return active_buffer_; }
        const std::vector<CompressedSegment> getCompressedSegments() const { return compressed_segments_; }

    private:
        // ---------- 内部工具函数 ----------

        // @brief 压缩活跃缓冲区
        void compressActiveBuffer();

        /**
         * @brief 解压指定段
         * @param idx 段索引
         * @return 解压后的价格点列表
         */
        std::vector<PricePoint> decompressSegment(size_t idx) const;

        /**
         * @brief 二分查找包含目标时间的段
         * @return 段索引，-1表示不存在
         */
        int findSegment(int64_t timestamp) const;

        /**
         * @brief 在压缩段中查找时间范围
         */
        std::pair<size_t, size_t> findSegmentRange(int64_t start, int64_t end) const;

        // @brief 检查价格点是否有效
        bool isValidPricePoint(int64_t timestamp, double price) const;
    };

} // namespace high_frequency_storage