/**
 * @file include/StorageEngine.hpp
 * @brief 存储引擎主类定义
 * 该文件定义了 StorageEngine 类，作为高频股票数据存储系统的核心组件。
 * StorageEngine 管理所有股票的时间序列数据，提供数据导入、查询、统计等功能。
 * 
 * @author beta dust
 * @date [2026-02-17]
*/  
#pragma once

#include "StockSeries.hpp"
#include "utils/TimeUtils.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <shared_mutex>

namespace high_frequency_storage {

    /**
     * @brief 存储引擎主类
     *
     * 管理所有股票的时间序列数据，提供数据导入、查询、统计等功能。
     * 作为整个系统的门面，所有外部操作都通过此类进行。
     */
    class StorageEngine {
    private:
        // ---------- 核心数据结构 ----------

        // 股票代码 -> 股票时间序列
        std::unordered_map<std::string, std::unique_ptr<StockSeries>> series_map_;

        // ---------- 元数据 ----------

        std::string name_;                    // 引擎名称
        size_t total_points_ = 0;              // 总数据点数
        std::atomic<bool> is_initialized_{ false };  // 是否已初始化

        // ---------- 并发控制 ----------

        mutable std::shared_mutex rw_mutex_;   // 读写锁（读共享，写独占）

        // ---------- 配置选项 ----------

        struct Config {
            bool auto_sort = true;              // 是否自动排序
            size_t reserve_size = 10000;        // 每只股票预分配大小
            bool enable_stats = true;            // 是否启用统计
        } config_;

    public:
        // ---------- 构造函数/析构函数 ----------

        /**
         * @brief 构造存储引擎
         * @param name 引擎名称
         */
        explicit StorageEngine(const std::string& name = "default");

        /**
         * @brief 析构函数
         */
        ~StorageEngine() = default;

        // 禁止拷贝（资源管理）
        StorageEngine(const StorageEngine&) = delete;
        StorageEngine& operator=(const StorageEngine&) = delete;

        // 允许移动
        StorageEngine(StorageEngine&&) = default;
        StorageEngine& operator=(StorageEngine&&) = default;

        // ---------- 初始化与配置 ----------

        /**
         * @brief 初始化引擎
         * @param reserve_size 每只股票预分配大小
         */
        void initialize(size_t reserve_size = 10000);

        /**
         * @brief 检查是否已初始化
         */
        bool isInitialized() const { return is_initialized_; }

        /**
         * @brief 设置配置项
         */
        void setConfig(const Config& config) { config_ = config; }

        /**
         * @brief 获取当前配置
         */
        Config getConfig() const { return config_; }

        // ---------- 数据导入接口 ----------

        /**
         * @brief 从 CSV 文件加载数据
         * @param filename CSV 文件路径
         * @param has_header 是否包含表头
         * @return 成功加载的数据点数
         *
         * CSV 格式要求：
         * - 列顺序：stock_code, timestamp, price
         * - 时间戳格式：Unix 毫秒时间戳 或 "YYYY-MM-DD HH:MM:SS.mmm"
         * - 价格格式：浮点数
         */

        size_t loadFromCSV(const std::string& filename, bool has_header = true);

        /**
         * @brief 从 CSV 字符串加载数据
         * @param csv_data CSV 格式的字符串
         * @return 成功加载的数据点数
         */
        
        size_t loadFromCSVString(const std::string& csv_data);

        /**
         * @brief 批量添加数据点
         * @param stock_code 股票代码
         * @param points 价格点列表
         */
        void addPoints(const std::string& stock_code,
            const std::vector<PricePoint>& points);

        /**
         * @brief 添加单个数据点
         * @param stock_code 股票代码
         * @param timestamp 时间戳
         * @param price 价格
         */
        void addPoint(const std::string& stock_code,
            int64_t timestamp, double price);

        // ---------- 查询接口 ----------

        /**
         * @brief 查询指定股票在时间范围内的价格
         * @param stock_code 股票代码
         * @param start_time 开始时间（包含）
         * @param end_time 结束时间（包含）
         * @return 价格列表（按时间升序）
         * @throws std::out_of_range 如果股票不存在
         */
        std::vector<double> queryRange(const std::string& stock_code,
            int64_t start_time,
            int64_t end_time) const;

        /**
         * @brief 查询多个股票在时间范围内的价格
         * @param stock_codes 股票代码列表
         * @param start_time 开始时间
         * @param end_time 结束时间
         * @return 股票代码 -> 价格列表 的映射
         */
        std::unordered_map<std::string, std::vector<double>>
            queryMultiRange(const std::vector<std::string>& stock_codes,
                int64_t start_time,
                int64_t end_time) const;

        /**
         * @brief 获取指定时间点的价格（最接近的）
         * @param stock_code 股票代码
         * @param timestamp 目标时间戳
         * @return 价格，如果无数据返回 0.0
         */
        double getPriceAt(const std::string& stock_code, int64_t timestamp) const;

        /**
         * @brief 获取股票的所有时间戳
         * @param stock_code 股票代码
         * @return 时间戳列表
         */
        std::vector<int64_t> getAllTimestamps(const std::string& stock_code) const;

        // ---------- 统计信息接口 ----------

        /**
         * @brief 获取所有股票代码
         */
        std::vector<std::string> getAllStockCodes() const;

        /**
         * @brief 获取股票数量
         */
        size_t stockCount() const { return series_map_.size(); }

        /**
         * @brief 获取总数据点数
         */
        size_t totalPoints() const { return total_points_; }

        /**
         * @brief 获取指定股票的数据点数
         * @param stock_code 股票代码
         * @throws std::out_of_range 如果股票不存在
         */
        size_t pointCount(const std::string& stock_code) const;

        /**
         * @brief 获取指定股票的最早时间戳
         */
        int64_t minTimestamp(const std::string& stock_code) const;

        /**
         * @brief 获取指定股票的最晚时间戳
         */
        int64_t maxTimestamp(const std::string& stock_code) const;

        /**
         * @brief 获取指定股票的最低价
         */
        double minPrice(const std::string& stock_code) const;

        /**
         * @brief 获取指定股票的最高价
         */
        double maxPrice(const std::string& stock_code) const;

        /**
         * @brief 检查股票是否存在
         */
        bool hasStock(const std::string& stock_code) const;

        // ---------- 数据维护接口 ----------

        /**
         * @brief 清空所有数据
         */
        void clear();

        /**
         * @brief 移除指定股票
         * @param stock_code 股票代码
         * @return 是否成功移除
         */
        bool removeStock(const std::string& stock_code);

        /**
         * @brief 对指定股票进行压缩优化
         * @param stock_code 股票代码
         */
        
        //void compress(const std::string& stock_code);

        /**
         * @brief 对所有股票进行压缩优化
         */
        
         //void compressAll();

        // ---------- 序列化接口 ----------

        /**
         * @brief 导出所有数据到 CSV 字符串
         */
        
        //std::string toCSV() const;

        /**
         * @brief 导出指定股票到 CSV 字符串
         */
        
        //std::string toCSV(const std::string& stock_code) const;

        /**
         * @brief 保存到文件
         * @param filename 文件名
         */
        
        //bool saveToFile(const std::string& filename) const;

        /**
         * @brief 从文件加载
         * @param filename 文件名
         */
        
        //bool loadFromFile(const std::string& filename);

        // ---------- 调试接口 ----------

        /**
         * @brief 打印引擎状态
         */
        void printStatus() const;

        /**
         * @brief 打印指定股票的头部数据
         * @param stock_code 股票代码
         * @param n 显示条数
         */
        void printHead(const std::string& stock_code, size_t n = 5) const;

        /**
         * @brief 获取内存占用估算值（字节）
         */
        size_t memoryUsage() const;

        /**
         * @brief 获取引擎名称
         */
        const std::string& name() const { return name_; }
    };

} // namespace high_frequency_storage