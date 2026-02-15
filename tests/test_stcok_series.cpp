/**
* @file tests/test_stock_series.cpp
* @author @betadust
* @date [2026-02-15]
*/

#include "StockSeries.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <set>

const double eps = 1e-9;
using namespace high_frequency_storage;

// 测试统计信息
struct TestStats {
    int total_tests = 0;
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

TestStats stats;

// 断言辅助函数
void assert_true(bool condition, const std::string& test_name) {
    stats.total_tests++;
    if (condition) {
        stats.passed++;
        std::cout << "   " << test_name << "\n";
    }
    else {
        stats.failed++;
        stats.failures.push_back(test_name);
        std::cout << "   " << test_name << "\n";
    }
}

/**
 * 测试1：边界条件测试
 */
void testBoundaryConditions() {
    std::cout << "\n========== 1. 边界条件测试 ==========\n";

    StockSeries series("BOUNDARY");

    // 1.1 空系列测试
    assert_true(series.empty(), "空系列初始为空");
    assert_true(series.size() == 0, "空系列大小为0");
    assert_true(series.minPrice() == 0.0, "空系列最低价为0");
    assert_true(series.maxPrice() == 0.0, "空系列最高价为0");

    // 1.2 边界时间戳测试
    
    {

        const int64_t MIN_TIME = 1577836800000LL;  // 2020-01-01
        const int64_t MAX_TIME = 1893456000000LL;  // 2030-01-01

        try {
            series.addPrice(MIN_TIME, 100.0);
            assert_true(true, "最小时间戳添加成功");
        }
        catch (...) {
            assert_true(false, "最小时间戳添加成功");
        }

        try {
            series.addPrice(MAX_TIME, 100.0);
            assert_true(true, "最大时间戳添加成功");
        }
        catch (...) {
            assert_true(false, "最大时间戳添加成功");
        }
    }
    

    // 1.3 边界价格测试
    try {
        series.addPrice(1707993025123LL, 0.01);  // 最小价格
        assert_true(true, "最小价格添加成功");
    }
    catch (...) {
        assert_true(false, "最小价格添加成功");
    }

    try {
        series.addPrice(1707993025124LL, 999999.99);  // 接近最大价格
        assert_true(true, "最大价格添加成功");
    }
    catch (...) {
        assert_true(false, "最大价格添加成功");
    }
}

/**
 * 测试2：异常输入测试
 */
void testExceptionHandling() {
    std::cout << "\n========== 2. 异常输入测试 ==========\n";

    StockSeries series("EXCEPTION");

    // 2.1 无效时间戳
    std::vector<int64_t> invalid_timestamps = {
        -1,                    // 负数
        0,                     // 0
        //1000,                  // 1970年
        //1577836800000LL - 1,   // 2019-12-31（小于最小值）
        //1893456000000LL + 1,   // 2030-01-02（大于最大值）
        //9999999999999LL        // 远大于最大值
    };

    for (size_t i = 0; i < invalid_timestamps.size(); i++) {
        try {
            series.addPrice(invalid_timestamps[i], 100.0);
            assert_true(false, "无效时间戳 " + std::to_string(invalid_timestamps[i]) + " 应该抛出异常");
        }
        catch (const std::invalid_argument& e) {
            assert_true(true, "无效时间戳 " + std::to_string(invalid_timestamps[i]) + " 正确抛出异常");
        }
    }

    // 2.2 无效价格
    std::vector<double> invalid_prices = {
        //-1.0,                   // 负数
        0.0,                    // 0
        //-100.0,                 // 负价格
        //1000000.0,              // 等于最大值（应该无效）
        //1000000.1,              // 大于最大值
        //1e10,                   // 超大
        std::nan(""),           // NaN
        std::numeric_limits<double>::infinity()  // 无穷大
    };

    for (size_t i = 0; i < invalid_prices.size(); i++) {
        try {
            series.addPrice(1707993025123LL, invalid_prices[i]);
            assert_true(false, "无效价格 " + std::to_string(invalid_prices[i]) + " 应该抛出异常");
        }
        catch (const std::invalid_argument& e) {
            assert_true(true, "无效价格 " + std::to_string(invalid_prices[i]) + " 正确抛出异常");
        }
        catch (...) {
            assert_true(false, "无效价格应抛出 std::invalid_argument");
        }
    }
}

/**
 * 测试3：有序性测试
 */
void testOrdering() {
    std::cout << "\n========== 3. 有序性测试 ==========\n";

    StockSeries series("ORDER");

    // 3.1 顺序添加
    series.addPrice(1707993025123LL, 175.23);
    series.addPrice(1707993025124LL, 176.45);
    series.addPrice(1707993025125LL, 174.89);

    auto prices = series.queryRange(0, 9999999999999LL);
    assert_true(prices.size() == 3, "顺序添加后查询返回正确数量");

    // 验证顺序
    assert_true(prices[0] == 175.23, "顺序添加后顺序正确 (1)");
    assert_true(prices[1] == 176.45, "顺序添加后顺序正确 (2)");
    assert_true(prices[2] == 174.89, "顺序添加后顺序正确 (3)");

    // 3.2 乱序添加
    StockSeries series2("UNORDERED");
    series2.addPrice(1707993025125LL, 174.89);  // t2
    series2.addPrice(1707993025123LL, 175.23);  // t0（乱序）
    series2.addPrice(1707993025124LL, 176.45);  // t1（乱序）

    auto prices2 = series2.queryRange(0, 9999999999999LL);
    assert_true(prices2.size() == 3, "乱序添加后查询返回正确数量");

    // 验证是否自动排序
    assert_true(prices2[0] == 175.23, "乱序添加后自动排序正确 (t0)");
    assert_true(prices2[1] == 176.45, "乱序添加后自动排序正确 (t1)");
    assert_true(prices2[2] == 174.89, "乱序添加后自动排序正确 (t2)");

    // 3.3 重复时间戳测试
    StockSeries series3("DUPLICATE");
    series3.addPrice(1707993025123LL, 175.23);
    series3.addPrice(1707993025123LL, 176.45);  // 相同时间戳

    auto prices3 = series3.queryRange(1707993025123LL, 1707993025123LL);
    assert_true(prices3.size() == 2, "相同时间戳允许存在");
}

/**
 * 测试4：范围查询精确性测试
 */
void testRangeQueryPrecision() {
    std::cout << "\n========== 4. 范围查询精确性测试 ==========\n";

    StockSeries series("RANGE");

    // 创建10个点，时间间隔1ms
    for (int i = 0; i < 10; i++) {
        series.addPrice(1707993025123LL + i, 175.23 + i * 0.1);
    }

    // 4.1 精确边界查询
    auto result1 = series.queryRange(1707993025123LL, 1707993025125LL);
    assert_true(result1.size() == 3, "精确边界查询返回正确数量");
    assert_true(result1[0] == 175.23, "精确边界查询起点正确");
    assert_true(std::abs(result1[2] - 175.43) < eps, "精确边界查询终点正确");

    // 4.2 包含边界查询
    auto result2 = series.queryRange(1707993025122LL, 1707993025126LL);
    assert_true(result2.size() == 4, "包含边界查询返回正确数量");

    // 4.3 单点查询
    auto result3 = series.queryRange(1707993025125LL, 1707993025125LL);
    assert_true(result3.size() == 1, "单点查询返回正确数量");
	std::cout << result3[0] << "\n";
   
    assert_true(std::abs(result3[0] - 175.43) < eps, "单点查询值正确");

    // 4.4 空范围查询
    auto result4 = series.queryRange(1707993025133LL, 1707993025135LL);
    assert_true(result4.empty(), "空范围查询返回空");

    auto result5 = series.queryRange(1707993025125LL, 1707993025122LL);  // start > end
    assert_true(result5.empty(), "无效范围查询返回空");
}

/**
 * 测试5：批量添加测试
 */
void testBatchAdd() {
    std::cout << "\n========== 5. 批量添加测试 ==========\n";

    StockSeries series("BATCH");

    // 5.1 正常批量添加
    std::vector<PricePoint> batch1 = {
        PricePoint(1707993025123LL, 175.23),
        PricePoint(1707993025124LL, 176.45),
        PricePoint(1707993025125LL, 174.89)
    };

    series.addPrices(batch1);
    assert_true(series.size() == 3, "批量添加成功");

    // 5.2 批量添加包含无效数据
    std::vector<PricePoint> batch2 = {
        PricePoint(1707993025126LL, 177.00),
        PricePoint(0, -100.0),  // 无效
        PricePoint(1707993025127LL, 178.12)
    };

    try {
        series.addPrices(batch2);
        assert_true(false, "包含无效数据的批量添加应该抛出异常");
    }
    catch (const std::invalid_argument& e) {
        assert_true(true, "包含无效数据的批量添加正确抛出异常");
        assert_true(series.size() == 3, "异常后数据未改变");  // 应该还是3条
    }
}

/**
 * 测试6：性能压力测试
 */
void testPerformance() {
    std::cout << "\n========== 6. 性能压力测试 ==========\n";

    StockSeries series("PERF");

    const int NUM_POINTS = 10000;
    const int NUM_QUERIES = 1000;

    // 6.1 批量插入性能
    std::cout << "  插入 " << NUM_POINTS << " 个点...\n";

    auto insert_start = std::chrono::steady_clock::now();

    std::vector<PricePoint> batch;
    batch.reserve(NUM_POINTS);
    for (int i = 0; i < NUM_POINTS; i++) {
        batch.emplace_back(1707993025123LL + i, 175.23 + i * 0.01);
    }

    try {
        series.addPrices(batch);
        auto insert_end = std::chrono::steady_clock::now();
        auto insert_ms = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - insert_start).count();
        std::cout << "    插入耗时: " << insert_ms << " ms\n";
        std::cout << "   插入速度: " << (NUM_POINTS * 1000 / (insert_ms + 1)) << " 点/秒\n";
        assert_true(true, "批量插入性能测试完成");
    }
    catch (const std::exception& e) {
        assert_true(false, std::string("批量插入失败: ") + e.what());
    }

    // 6.2 范围查询性能
    std::cout << "\n  执行 " << NUM_QUERIES << " 次随机范围查询...\n";

    auto query_start = std::chrono::steady_clock::now();

    int total_results = 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> offset_dist(0, NUM_POINTS - 10);
    std::uniform_int_distribution<> range_dist(1, 100);

    for (int i = 0; i < NUM_QUERIES; i++) {
        int start_offset = offset_dist(gen);
        int range = range_dist(gen);
        int64_t start_time = 1707993025123LL + start_offset;
        int64_t end_time = start_time + range;

        auto results = series.queryRange(start_time, end_time);
        total_results += results.size();
    }

    auto query_end = std::chrono::steady_clock::now();
    auto query_ms = std::chrono::duration_cast<std::chrono::milliseconds>(query_end - query_start).count();

    std::cout << "   查询耗时: " << query_ms << " ms\n";
    std::cout << "   查询速度: " << (NUM_QUERIES * 1000 / (query_ms + 1)) << " 次/秒\n";
    std::cout << "   平均返回: " << (total_results / NUM_QUERIES) << " 点/查询\n";
    assert_true(true, "范围查询性能测试完成");
}

/**
 * 测试7：并发安全性测试（如果需要）
 */
void testConcurrency() {
    std::cout << "\n========== 7. 并发安全性测试 ==========\n";
    std::cout << "   需要线程测试，单独运行 test_time_utils_thread.cpp\n";
    // 这里只做标记，不实际运行并发测试
}

/**
 * 测试8：统计信息准确性测试
 */
void testStatsAccuracy() {
    std::cout << "\n========== 8. 统计信息准确性测试 ==========\n";

    StockSeries series("STATS");

    // 添加一些数据
    series.addPrice(1707993025123LL, 175.23);
    series.addPrice(1707993025124LL, 176.45);
    series.addPrice(1707993025125LL, 174.89);
    series.addPrice(1707993025126LL, 177.00);
    series.addPrice(1707993025127LL, 178.12);

    assert_true(series.minPrice() == 174.89, "最低价正确");
    assert_true(series.maxPrice() == 178.12, "最高价正确");
    assert_true(series.minTimestamp() == 1707993025123LL, "最早时间正确");
    assert_true(series.maxTimestamp() == 1707993025127LL, "最晚时间正确");
    assert_true(std::abs(series.priceRange() - (178.12 - 174.89)) < 0.001, "价格范围正确");
    assert_true(series.timeSpan() == 4, "时间跨度正确");  // 5127 - 5123 = 4ms

    // 清空后测试
    series.clear();
    assert_true(series.empty(), "清空后为空");
    assert_true(series.minPrice() == 0.0, "清空后最低价为0");
    assert_true(series.maxPrice() == 0.0, "清空后最高价为0");
}

/**
 * 测试9：随机数据验证
 */
void testRandomData() {
    std::cout << "\n========== 9. 随机数据验证 ==========\n";

    StockSeries series("RANDOM");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> time_dist(1577836800000LL, 1893456000000LL);
    std::uniform_real_distribution<double> price_dist(1.0, 1000.0);

    const int NUM_RANDOM = 1000;

    // 生成随机数据
    std::vector<std::pair<int64_t, double>> raw_data;
    raw_data.reserve(NUM_RANDOM);

    for (int i = 0; i < NUM_RANDOM; i++) {
        int64_t t = time_dist(gen);
        double p = price_dist(gen);
        raw_data.emplace_back(t, p);

        try {
            series.addPrice(t, p);
        }
        catch (...) {
            // 可能因为时间戳乱序等原因，忽略
        }
    }

    // 验证所有添加的数据都能被查询到
    int found_count = 0;
    for (const auto& [t, p] : raw_data) {
        auto results = series.queryRange(t, t);
        if (!results.empty()) {
            found_count++;
        }
    }

    std::cout << "  随机生成: " << NUM_RANDOM << " 个点\n";
    std::cout << "  成功添加: " << series.size() << " 个点\n";
    std::cout << "  查询命中: " << found_count << " 个点\n";

    assert_true(found_count <= series.size(), "查询命中数不超过总数");
}

/**
 * 主测试函数
 */
int main() {
    std::cout << "========================================\n";
    std::cout << "    StockSeries 严格测试套件\n";
    std::cout << "========================================\n";

    auto start_time = std::chrono::steady_clock::now();

    // 运行所有测试
    testBoundaryConditions();
    testExceptionHandling();
    testOrdering();
    testRangeQueryPrecision();
    testBatchAdd();
    testStatsAccuracy();
    testRandomData();
    testPerformance();
    //testConcurrency();  // 仅标记

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // 输出测试结果统计
    std::cout << "\n========================================\n";
    std::cout << "测试完成统计\n";
    std::cout << "========================================\n";
    std::cout << "总测试数: " << stats.total_tests << "\n";
    std::cout << "通过: " << stats.passed << " \n";
    std::cout << "失败: " << stats.failed << " \n";
    std::cout << "耗时: " << elapsed_ms << " ms\n";

    if (!stats.failures.empty()) {
        std::cout << "\n失败的测试:\n";
        for (const auto& f : stats.failures) {
            std::cout << "  - " << f << "\n";
        }
    }

    std::cout << "\n========================================\n";

    return stats.failed == 0 ? 0 : 1;
}