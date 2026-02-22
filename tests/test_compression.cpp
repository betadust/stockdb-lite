/**
 * @file tests/test_compression.cpp
 * @author @betadust
 * @date [2026-02-21]
 *
 * @brief StockSeries 压缩功能测试
 *
 * 测试内容：
 * - 自动压缩触发测试
 * - 压缩后数据完整性测试
 * - 查询功能测试（跨压缩段、活跃缓冲区）
 * - 内存占用对比测试
 * - 边界条件测试
 * - 性能测试
 */

#include "StockSeries.hpp"
#include "Compressor.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <thread>

using namespace high_frequency_storage;

// 测试辅助函数：生成测试数据
std::vector<PricePoint> generateTestData(size_t count, int64_t start_time = 1707984000000LL, double base_price = 100.0) {
    std::vector<PricePoint> data;
    data.reserve(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_var(-2.0, 2.0);

    for (size_t i = 0; i < count; ++i) {
        int64_t timestamp = start_time + i * 60000;  // 每分钟一个点
        double price = base_price + price_var(gen);
        data.emplace_back(timestamp, price);
    }

    return data;
}

// 测试辅助函数：验证数据一致性
bool verifyDataConsistency(const StockSeries& series, const std::vector<PricePoint>& original) {
    // 验证总点数
    if (series.size() != original.size()) {
        std::cout << "点数不匹配: 系列=" << series.size()
            << ", 原始=" << original.size() << "\n";
        return false;
    }

    // 验证时间范围
    if (series.minTimestamp() != original.front().getTimestamp()) {
        std::cout << "最小时间戳不匹配\n";
        return false;
    }

    if (series.maxTimestamp() != original.back().getTimestamp()) {
        std::cout << "最大时间戳不匹配\n";
        return false;
    }

    // 随机抽查10个点
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, original.size() - 1);

    for (int i = 0; i < 10; ++i) {
        size_t idx = dist(gen);
        int64_t ts = original[idx].getTimestamp();
        double expected = original[idx].getPrice();
        double actual = series.getPriceAt(ts);

        if (std::abs(actual - expected) > 0.005) {
            std::cout << "点 " << idx << " (ts=" << ts
                << ") 不匹配: 期望=" << expected
                << ", 实际=" << actual << "\n";
            return false;
        }
    }

    return true;
}

// ========== 测试1：自动压缩触发测试 ==========
void testAutoCompression() {
    std::cout << "\n========== 测试1：自动压缩触发 ==========\n";

    StockSeries series("TEST");

    // 添加数据直到触发压缩
    size_t pre_compress_points = 0;
    for (int i = 1; i <= 15000; ++i) {
        series.addPrice(1000 + i, 100.0 + i * 0.01);

        // 检查是否触发了压缩
        if (i == 10000) {
            pre_compress_points = series.size();
            std::cout << "添加 " << i << " 个点后，压缩段数: "
                << series.getCompressedSegments().size() << "\n";
        }
    }

    // 验证压缩段被创建
    auto segments = series.getCompressedSegments();
    std::cout << "最终压缩段数: " << segments.size() << "\n";
    std::cout << "活跃缓冲区大小: " << series.getActiveBuffer().size() << "\n";
    std::cout << "总点数: " << series.size() << "\n";

    assert(!segments.empty());
    assert(series.size() == 15000);

    std::cout << " 自动压缩触发测试通过\n";
}

// ========== 测试2：压缩数据完整性测试 ==========
void testDataIntegrity() {
    std::cout << "\n========== 测试2：数据完整性测试 ==========\n";

    // 生成测试数据
    auto original = generateTestData(25000, 1000000, 100.0);

    StockSeries series("TEST");

    // 批量添加数据
    auto start = std::chrono::high_resolution_clock::now();
    series.addPrices(original);
    auto mid = std::chrono::high_resolution_clock::now();

    // 强制压缩剩余数据
    series.flush();
    auto end = std::chrono::high_resolution_clock::now();

    auto add_ms = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start);
    auto compress_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid);

    std::cout << "添加数据耗时: " << add_ms.count() << " ms\n";
    std::cout << "压缩耗时: " << compress_ms.count() << " ms\n";

    // 验证数据完整性
    bool consistent = verifyDataConsistency(series, original);
    assert(consistent);

    std::cout << " 数据完整性测试通过\n";
}

// ========== 测试3：查询功能测试 ==========
void testQueryFunctions() {
    std::cout << "\n========== 测试3：查询功能测试 ==========\n";

    StockSeries series("TEST");

    // 添加数据，确保跨多个压缩段
    for (int i = 1; i <= 35000; ++i) {
        series.addPrice(1000 + i * 10, 100.0 + i * 0.01);
    }

    auto segments = series.getCompressedSegments();
    std::cout << "压缩段数: " << segments.size() << "\n";
    std::cout << "活跃缓冲区大小: " << series.getActiveBuffer().size() << "\n";

    // 测试1：查询整个范围
    auto all_prices = series.queryRange(1000, 1000 + 35000 * 10);
    std::cout << "全范围查询结果数: " << all_prices.size() << "\n";
    assert(all_prices.size() == 35000);

    // 测试2：查询单个压缩段范围
    if (!segments.empty()) {
        int64_t seg_start = segments[0].min_timestamp;
        int64_t seg_end = segments[0].max_timestamp;
        auto seg_prices = series.queryRange(seg_start, seg_end);
        std::cout << "第一个压缩段查询结果数: " << seg_prices.size() << "\n";
        assert(seg_prices.size() == segments[0].block.count);
    }

    // 测试3：查询跨压缩段的范围
    if (segments.size() >= 2) {
        int64_t cross_start = segments[0].min_timestamp;
        int64_t cross_end = segments[1].max_timestamp;
        auto cross_prices = series.queryRange(cross_start, cross_end);
        size_t expected = segments[0].block.count + segments[1].block.count;
        std::cout << "跨段查询结果数: " << cross_prices.size()
            << " (期望: " << expected << ")\n";
        assert(cross_prices.size() == expected);
    }

    // 测试4：查询压缩段和活跃缓冲区交叉范围
    if (!series.getActiveBuffer().empty()) {
        int64_t last_seg_end = segments.back().max_timestamp;
        int64_t active_start = series.getActiveBuffer().front().getTimestamp();
        int64_t active_end = series.getActiveBuffer().back().getTimestamp();

        auto mixed_prices = series.queryRange(last_seg_end - 1000, active_end);
        std::cout << "混合查询结果数: " << mixed_prices.size() << "\n";
        assert(mixed_prices.size() > 0);
    }

    // 测试5：边界查询
    auto empty1 = series.queryRange(1000, 999);  // start > end
    assert(empty1.empty());

    auto empty2 = series.queryRange(1, 999);  // 范围在数据之前
    assert(empty2.empty());

    auto empty3 = series.queryRange(1000 + 35000 * 10 + 1000,  // 范围在数据之后
        1000 + 35000 * 10 + 2000);
    assert(empty3.empty());

    std::cout << " 查询功能测试通过\n";
}

// ========== 测试4：内存占用对比测试 ==========
void testMemoryUsage() {
    std::cout << "\n========== 测试4：内存占用对比 ==========\n";

    const size_t POINTS = 1000000;

    // 生成测试数据
    auto data = generateTestData(POINTS, 1000000, 100.0);

    // 测试无压缩的原始内存占用
    size_t raw_memory = data.capacity() * sizeof(PricePoint);
    std::cout << "原始数据内存: " << raw_memory / 1024.0 << " KB\n";

    // 测试压缩后的内存占用
    StockSeries series("TEST");
    series.addPrices(data);
    series.flush();  // 强制压缩所有数据

    size_t compressed_memory = series.memoryUsage();
    std::cout << "压缩后内存: " << compressed_memory / 1024.0 << " KB\n";
    std::cout << "压缩率: " << std::fixed << std::setprecision(2)
        << (100.0 * compressed_memory / raw_memory) << "%\n";

    // 验证数据完整性
    assert(verifyDataConsistency(series, data));

    // 内存应该明显减少
    assert(compressed_memory < raw_memory * 0.7);  // 至少节省70%

    std::cout << " 内存占用测试通过\n";
}

// ========== 测试5：边界条件测试 ==========
void testEdgeCases() {
    std::cout << "\n========== 测试5：边界条件测试 ==========\n";

    StockSeries series("TEST");

    // 测试1：空系列
    std::cout << series.size() << "\n";
    assert(series.size() == 0);
    assert(series.getActiveBuffer().empty());
    assert(series.getCompressedSegments().empty());
    assert(series.minPrice() == std::numeric_limits<double>::max());
    assert(series.maxPrice() == std::numeric_limits<double>::min());

    // 测试2：单个点
    series.addPrice(1000, 100.0);
    assert(series.size() == 1);
    assert(series.minPrice() == 100.0);
    assert(series.maxPrice() == 100.0);
    assert(series.getPriceAt(1000) == 100.0);

    // 测试3：刚好达到压缩阈值
    for (int i = 2; i <= 10000; ++i) {
        series.addPrice(1000 + i * 10, 100.0 + i);
    }
    assert(series.getCompressedSegments().size() >= 1);

    // 测试4：查询不存在的点
    assert(series.getPriceAt(999) == 0.0);
    assert(series.getPriceAt(1000 + 10001 * 10) == 0.0);

    // 测试5：清空后重新添加
    series.clear();
    assert(series.size() == 0);
    assert(series.getActiveBuffer().empty());
    assert(series.getCompressedSegments().empty());

    series.addPrice(2000, 200.0);
    assert(series.size() == 1);
    assert(series.getPriceAt(2000) == 200.0);

    std::cout << " 边界条件测试通过\n";
}

// ========== 测试6：异常处理测试 ==========
void testExceptionHandling() {
    std::cout << "\n========== 测试6：异常处理测试 ==========\n";

    StockSeries series("TEST");

    // 测试无效价格
    try {
        series.addPrice(1000, -10.0);
        assert(false);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "捕获负价格异常: " << e.what() << "\n";
    }

    try {
        series.addPrice(1000, std::nan(""));
        assert(false);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "捕获 NaN 异常\n";
    }

    try {
        series.addPrice(1000, std::numeric_limits<double>::infinity());
        assert(false);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "捕获 Inf 异常\n";
    }

    // 测试时间戳非递增
    series.addPrice(1000, 100.0);

    try {
        series.addPrice(999, 101.0);  // 时间戳减小
        assert(false);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "捕获时间戳非递增异常: " << e.what() << "\n";
    }

    try {
        series.addPrice(1000, 102.0);  // 时间戳相等
        assert(false);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "捕获时间戳相等异常\n";
    }

    // 测试批量添加中的异常
    std::vector<PricePoint> bad_batch = {
        PricePoint(2000, 200.0),
        PricePoint(1999, 201.0),  // 乱序
        PricePoint(2001, 202.0)
    };

    try {
        series.addPrices(bad_batch);
        assert(false);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "捕获批量乱序异常\n";
    }

    // 验证数据没有被破坏
    assert(series.size() == 1);
    assert(series.getPriceAt(1000) == 100.0);

    std::cout << " 异常处理测试通过\n";
}

// ========== 测试7：性能压力测试 ==========
void testPerformance() {
    std::cout << "\n========== 测试7：性能压力测试 ==========\n";

    const size_t TOTAL_POINTS = 500000;
    const int NUM_QUERIES = 10000;

    // 生成测试数据
    auto data = generateTestData(TOTAL_POINTS, 1000000, 100.0);

    StockSeries series("TEST");

    // 测试添加性能
    auto add_start = std::chrono::high_resolution_clock::now();
    series.addPrices(data);
    auto add_end = std::chrono::high_resolution_clock::now();

    auto add_ms = std::chrono::duration_cast<std::chrono::milliseconds>(add_end - add_start);
    std::cout << "添加 " << TOTAL_POINTS << " 点耗时: " << add_ms.count() << " ms\n";
    std::cout << "添加速度: " << (TOTAL_POINTS * 1000.0 / add_ms.count()) << " 点/秒\n";

    // 测试查询性能
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, TOTAL_POINTS - 1);

    auto query_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_QUERIES; ++i) {
        size_t idx = dist(gen);
        int64_t ts = data[idx].getTimestamp();
        double price = series.getPriceAt(ts);
        assert(std::abs(price - data[idx].getPrice()) < 0.005);
    }

    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_us = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);

    std::cout << NUM_QUERIES << " 次点查询耗时: " << query_us.count() << " μs\n";
    std::cout << "平均查询时间: " << query_us.count() / NUM_QUERIES << " μs\n";

    // 测试范围查询性能
    auto range_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        size_t start_idx = dist(gen);
        size_t end_idx = std::min(start_idx + 1000, TOTAL_POINTS - 1);
        int64_t start_ts = data[start_idx].getTimestamp();
        int64_t end_ts = data[end_idx].getTimestamp();

        auto prices = series.queryRange(start_ts, end_ts);
        assert(prices.size() == end_idx - start_idx + 1);
    }

    auto range_end = std::chrono::high_resolution_clock::now();
    auto range_ms = std::chrono::duration_cast<std::chrono::milliseconds>(range_end - range_start);

    std::cout << "100 次范围查询耗时: " << range_ms.count() << " ms\n";
    std::cout << "平均范围查询时间: " << range_ms.count() / 100.0 << " ms\n";

    std::cout << " 性能测试通过\n";
}

// ========== 主测试函数 ==========
int main() {
    std::cout << "========================================\n";
    std::cout << "    StockSeries 压缩功能完整测试\n";
    std::cout << "========================================\n";

    try {
        testAutoCompression();
        testDataIntegrity();
        testQueryFunctions();
        testMemoryUsage();
        testEdgeCases();
        testExceptionHandling();
        testPerformance();

        std::cout << "\n========================================\n";
        std::cout << " 所有压缩测试通过！\n";
        std::cout << "========================================\n";

    }
    catch (const std::exception& e) {
        std::cerr << "\n 测试失败: " << e.what() << "\n";
        return 1;
    }

    return 0;
}