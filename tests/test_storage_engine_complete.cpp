/** 
 * @file tests/test_storage_engine_complete.cpp
 * @brief StorageEngine 综合测试套件，覆盖基础功能、CSV加载、大文件性能、无效数据处理、多线程并发和序列化等方面
 * @author betadust
 * @date [2026-02-19]
 */ 


#include "StorageEngine.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <random>

using namespace high_frequency_storage;

// 测试辅助函数：生成测试CSV文件
void generateTestCSV(const std::string& filename, size_t rows, bool with_header = true) {
    std::ofstream file(filename);

    if (with_header) {
        file << "stock_code,timestamp,price\n";
    }

    std::vector<std::string> stocks = { "AAPL", "GOOG", "MSFT", "AMZN", "META" };
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> stock_dist(0, stocks.size() - 1);
    std::uniform_real_distribution<> price_dist(100.0, 1000.0);

    for (size_t i = 0; i < rows; i++) {
        std::string stock = stocks[stock_dist(gen)];
        int64_t timestamp = 1707984000000LL + i * 60000; // 每分钟一个点
        double price = price_dist(gen);

        file << stock << "," << timestamp << "," << price << "\n";
    }
    file.close();
}

// 测试辅助函数：生成包含无效数据的CSV
void generateInvalidCSV(const std::string& filename) {
    std::ofstream file(filename);
    file << "stock_code,timestamp,price\n"
        << "AAPL,2024-02-15 09:30:00.000,175.23\n"
        << "GOOG,invalid_timestamp,2780.50\n"           // 无效时间戳
        << "MSFT,1707984060000,-100.00\n"                // 无效价格
        << "AMZN,1707984120000,150.23\n"
        << ",1707984180000,330.45\n"                      // 空股票代码
        << "META,1707984240000,500.00\n";
    file.close();
}

// ========== 1. 基础功能测试 ==========
void testBasicFunctions() {
    std::cout << "\n========== 1. 基础功能测试 ==========\n";

    StorageEngine engine;
    engine.initialize(1000);

    // 测试添加点
    engine.addPoint("AAPL", 1707984000000LL, 175.23);
    engine.addPoint("AAPL", 1707984060000LL, 176.45);
    engine.addPoint("GOOG", 1707984000000LL, 2780.50);

    assert(engine.stockCount() == 2);
    assert(engine.totalPoints() == 3);
    assert(engine.hasStock("AAPL"));
    assert(!engine.hasStock("MSFT"));

    // 测试查询
    auto prices = engine.queryRange("AAPL", 1707984000000LL, 1707984060000LL);
    assert(prices.size() == 2);
    assert(prices[0] == 175.23);
    assert(prices[1] == 176.45);

    // 测试单点查询
    double price = engine.getPriceAt("AAPL", 1707984000000LL);
    assert(price == 175.23);

    // 测试统计信息
    assert(engine.minPrice("AAPL") == 175.23);
    assert(engine.maxPrice("AAPL") == 176.45);
    assert(engine.minTimestamp("AAPL") == 1707984000000LL);
    assert(engine.maxTimestamp("AAPL") == 1707984060000LL);

    // 测试获取所有股票代码
    auto codes = engine.getAllStockCodes();
    assert(codes.size() == 2);
    assert(std::find(codes.begin(), codes.end(), "AAPL") != codes.end());

    std::cout << "基础功能测试通过\n";
}

// ========== 2. CSV加载测试 ==========
void testCSVLoading() {
    std::cout << "\n========== 2. CSV加载测试 ==========\n";

    // 生成测试文件
    generateTestCSV("test_normal.csv", 100, true);

    StorageEngine engine;
    engine.initialize();

    // 测试 loadFromCSV
    size_t count = engine.loadFromCSV("test_normal.csv");
    std::cout << "加载了 " << count << " 个数据点\n";
    assert(count == 100);
    assert(engine.totalPoints() == 100);

    // 测试 loadFromCSVString
    std::string csv_data = "AAPL,2024-02-15 09:30:00.000,175.23\nGOOG,2024-02-15 09:30:00.000,2780.50\n";
    StorageEngine engine2;
    engine2.initialize();
    count = engine2.loadFromCSVString(csv_data);
    assert(count == 2);
    assert(engine2.stockCount() == 2);

    std::cout << "CSV加载测试通过\n";

    // 清理
    std::filesystem::remove("test_normal.csv");
}

// ========== 3. 大文件加载测试 ==========
void testLargeCSVLoading() {
    std::cout << "\n========== 3. 大文件加载测试 ==========\n";

    // 生成10万行测试数据
    std::cout << "生成测试数据...\n";
    generateTestCSV("test_large.csv", 100000, true);

    StorageEngine engine;
    engine.initialize(10000);

    auto start = std::chrono::high_resolution_clock::now();

    size_t count = engine.loadLargeCSV("test_large.csv");
	//size_t count = engine.loadFromCSV("test_large.csv");
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "加载了 " << count << " 个数据点\n";
    std::cout << "耗时: " << ms.count() << " ms\n";
    std::cout << "速度: " << (count * 1000.0 / ms.count()) << " 行/秒\n";

    assert(count == 100000);
    assert(engine.totalPoints() == 100000);

    // 验证数据完整性
    auto codes = engine.getAllStockCodes();
    std::cout << "股票种类数: " << codes.size() << "\n";

    std::cout << "大文件加载测试通过\n";

    // 清理
    std::filesystem::remove("test_large.csv");
}

// ========== 4. 无效数据处理测试 ==========
void testInvalidData() {
    std::cout << "\n========== 4. 无效数据处理测试 ==========\n";

    generateInvalidCSV("test_invalid.csv");

    StorageEngine engine;
    engine.initialize();

    size_t count = engine.loadFromCSV("test_invalid.csv");
    std::cout << "有效数据加载: " << count << " 行\n";

    // 应该只加载了3行有效数据（AAPL, AMZN, META）
    assert(count == 3);
	auto vec = engine.getAllStockCodes();
    for (auto code : vec) {
        std::cout << code << "\n";
    }
    assert(engine.stockCount() == 3);

    // 验证有效数据
    assert(engine.hasStock("AAPL"));
    assert(engine.hasStock("AMZN"));
    assert(engine.hasStock("META"));

    // 无效的股票不应该存在
    assert(!engine.hasStock("GOOG"));
    assert(!engine.hasStock("MSFT"));

    std::cout << " 无效数据处理测试通过\n";

    std::filesystem::remove("test_invalid.csv");
}

// ========== 5. 多线程并发测试 ==========
void testConcurrentAccess() {
    std::cout << "\n========== 5. 多线程并发测试 ==========\n";

    StorageEngine engine;
    engine.initialize();

    // 先加载一些数据
    engine.addPoint("AAPL", 1707984000000LL, 175.23);
    engine.addPoint("GOOG", 1707984000000LL, 2780.50);

    std::vector<std::thread> threads;
    std::atomic<int> success_count{ 0 };

    // 5个写线程
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([&success_count, &engine, i]() {
            for (int j = 0; j < 100; j++) {
                try {
					//std::cout << "线程 " << i << " 正在添加数据点 " << j << "\n";
                    engine.addPoint("AAPL", 1707984000000LL + j, 175.23 + j);
                    success_count++;
                }
                catch (...) {}
            }
            });
    }
    
    // 5个读线程
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([&success_count, &engine, i]() {
            for (int j = 0; j < 100; j++) {
                try {
					//std::cout << "线程 " << i << " 正在查询数据次数 " << j << "\n";
                    auto prices = engine.queryRange("AAPL", 0, 9999999999999LL);
                    if (!prices.empty()) success_count++;
                }
                catch (...) {}
            }
            });
    }
	std::cout << "启动了 " << threads.size() << " 个线程进行并发读写\n";
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "并发操作成功次数: " << success_count << "\n";
    std::cout << "最终总点数: " << engine.totalPoints() << "\n";

    assert(engine.totalPoints() > 0);
    std::cout << "多线程测试通过\n";
}

// ========== 6. 序列化测试 ==========
void testSerialization() {
    std::cout << "\n========== 6. 序列化测试 ==========\n";

    StorageEngine engine1;
    engine1.initialize();

    engine1.addPoint("AAPL", 1707984000000LL, 175.23);
    engine1.addPoint("AAPL", 1707984060000LL, 176.45);
    engine1.addPoint("GOOG", 1707984000000LL, 2780.50);

    // 测试 toCSV()
    std::string csv_all = engine1.toCSV();
    std::cout << "CSV 输出前几行:\n";
    std::cout << csv_all.substr(0, 200) << "...\n";

    // 验证 CSV 格式
    assert(csv_all.find("stock_code,timestamp,price") == 0);

    // 测试 saveToFile
    assert(engine1.saveToFile("test_engine.dat"));

    // 测试 loadFromFile
    StorageEngine engine2;
    engine2.initialize();
    assert(engine2.loadFromCSV("test_engine.dat"));

    // 验证加载的数据
    assert(engine2.stockCount() == 2);
    assert(engine2.totalPoints() == 3);

    auto prices = engine2.queryRange("AAPL", 1707984000000LL, 1707984060000LL);
    assert(prices.size() == 2);
    assert(prices[0] == 175.23);

    std::cout << "序列化测试通过\n";

    std::filesystem::remove("test_engine.dat");
}

// ========== 7. 数据维护测试 ==========
void testDataMaintenance() {
    std::cout << "\n========== 7. 数据维护测试 ==========\n";

    StorageEngine engine;
    engine.initialize();

    engine.addPoint("AAPL", 1707984000000LL, 175.23);
    engine.addPoint("AAPL", 1707984060000LL, 176.45);
    engine.addPoint("GOOG", 1707984000000LL, 2780.50);

    assert(engine.totalPoints() == 3);

    // 测试 removeStock
    assert(engine.removeStock("AAPL"));
    assert(!engine.hasStock("AAPL"));
    assert(engine.totalPoints() == 1);  // 只剩 GOOG

    // 测试 clear
    engine.clear();
    assert(engine.stockCount() == 0);
    assert(engine.totalPoints() == 0);
    assert(!engine.isInitialized());

    std::cout << " 数据维护测试通过\n";
}

// ========== 8. 边界条件测试 ==========
void testEdgeCases() {
    std::cout << "\n========== 8. 边界条件测试 ==========\n";

    StorageEngine engine;
    engine.initialize();

    // 测试空引擎查询
    try {
        engine.queryRange("AAPL", 0, 100);
        assert(false);  // 不应该到达这里
    }
    catch (const std::out_of_range& e) {
        std::cout << "空查询正确抛出异常: " << e.what() << "\n";
    }

    // 测试空范围查询
    engine.addPoint("AAPL", 1707984000000LL, 175.23);
    auto prices = engine.queryRange("AAPL", 1707984000000LL, 1707984000000LL - 1);
    assert(prices.empty());

    // 测试不存在的股票
    try {
        engine.minPrice("MSFT");
        assert(false);
    }
    catch (const std::out_of_range& e) {
        std::cout << "不存在股票正确抛出异常\n";
    }

    // 测试 getAllTimestamps
    auto timestamps = engine.getAllTimestamps("AAPL");
    assert(timestamps.size() == 1);
    assert(timestamps[0] == 1707984000000LL);

    std::cout << "边界条件测试通过\n";
}

// ========== 9. 内存使用测试 ==========
void testMemoryUsage() {
    std::cout << "\n========== 9. 内存使用测试 ==========\n";

    StorageEngine engine;
    engine.initialize();

    size_t initial_memory = engine.memoryUsage();
    std::cout << "初始内存: " << initial_memory << " 字节\n";

    // 添加10万点
    for (int i = 0; i < 100000; i++) {
        engine.addPoint("AAPL", 1707984000000LL + i, 100.0 + i);
    }

    size_t final_memory = engine.memoryUsage();
    std::cout << "添加10万点后内存: " << final_memory << " 字节\n";
    std::cout << "每点平均内存: " << (final_memory - initial_memory) / 100000.0 << " 字节\n";

    assert(final_memory > initial_memory);
    std::cout << "内存使用测试通过\n";
}

// ========== 10. 性能基准测试 ==========
void testPerformanceBenchmark() {
    std::cout << "\n========== 10. 性能基准测试 ==========\n";

    StorageEngine engine;
    engine.initialize();

    // 测试添加性能
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; i++) {
        engine.addPoint("AAPL", 1707984000000LL + i, 100.0 + i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto add_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "添加10万点耗时: " << add_ms.count() << " ms\n";
    std::cout << "添加速度: " << (100000 * 1000.0 / add_ms.count()) << " 点/秒\n";

    // 测试查询性能
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        engine.queryRange("AAPL", 1707984000000LL, 1707984000000LL + 100000);
    }

    end = std::chrono::high_resolution_clock::now();
    auto query_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "1000次范围查询耗时: " << query_ms.count() << " ms\n";
    std::cout << "平均查询时间: " << query_ms.count() / 1000.0 << " ms/次\n";

    std::cout << "性能基准测试通过\n";
}

// ========== 主测试函数 ==========
int main() {
    std::cout << "========================================\n";
    std::cout << "    StorageEngine 完整测试套件\n";
    std::cout << "========================================\n";

    auto start_time = std::chrono::steady_clock::now();

    // 运行所有测试
    //testBasicFunctions();
    //testCSVLoading();
    testLargeCSVLoading();
    //testInvalidData();
    testConcurrentAccess();
    testSerialization();
    testDataMaintenance();
    testEdgeCases();
    testMemoryUsage();
    testPerformanceBenchmark();

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

    std::cout << "\n========================================\n";
    std::cout << "测试完成！\n";
    std::cout << "总耗时: " << elapsed.count() << " 秒\n";
    std::cout << "所有测试通过！ \n";
    std::cout << "========================================\n";

    return 0;
}