/**
 * @file examples/main.cpp
 * @brief StockDB-Lite 主示例程序
 *
 * 展示存储引擎的基本使用方法：
 * - 从 CSV 加载数据
 * - 添加实时数据
 * - 执行各种查询
 * - 查看统计信息
 * - 保存和加载数据
 */

#include "StorageEngine.hpp"
#include "utils/TimeUtils.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace high_frequency_storage;

// 辅助函数：打印分隔线
void printSeparator() {
    std::cout << "\n" << std::string(60, '=') << "\n\n";
}

// 辅助函数：将 Unix 毫秒时间戳转换为可读格式
std::string formatTime(int64_t ms) {
    return utils::TimeUtils::toString(ms);
}

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════╗\n";
    std::cout << "║         StockDB-Lite 演示程序                 ║\n";
    std::cout << "║     轻量级高频股票数据存储引擎                 ║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n\n";

    try {
        // ========== 1. 创建存储引擎 ==========
        std::cout << " 创建存储引擎...\n";
        StorageEngine engine("stock_demo");
        engine.initialize(10000);  // 每只股票预分配1万点

        std::cout << " 引擎初始化完成\n";
        printSeparator();

        // ========== 2. 从 CSV 加载数据 ==========
        std::cout << " 从 CSV 文件加载数据...\n";

        // 尝试加载示例数据
        std::string data_file = "../data/sample_stocks.csv";
        try {
            size_t loaded = engine.loadFromCSV(data_file);
            std::cout << " 成功加载 " << loaded << " 个数据点\n";
            std::cout << "   股票数量: " << engine.stockCount() << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "  无法加载示例数据: " << e.what() << "\n";
            std::cerr << "   请确保 data/sample_stocks.csv 文件存在\n";

            // 如果没有示例数据，创建一些测试数据
            std::cout << "   创建测试数据...\n";
            engine.addPoint("AAPL", 1707984000000LL, 175.23);
            engine.addPoint("AAPL", 1707984060000LL, 176.45);
            engine.addPoint("GOOG", 1707984000000LL, 2780.50);
            engine.addPoint("GOOG", 1707984060000LL, 2790.75);
            engine.addPoint("MSFT", 1707984000000LL, 330.45);
            engine.addPoint("MSFT", 1707984060000LL, 332.60);
        }

        engine.printStatus();
        printSeparator();

        // ========== 3. 添加实时数据 ==========
        std::cout << " 模拟实时数据添加...\n";

        // 获取当前时间作为基准
        int64_t now = utils::TimeUtils::now();

        // 添加几个新的数据点
        engine.addPoint("AAPL", now - 60000, 182.34);  // 1分钟前
        engine.addPoint("AAPL", now, 183.56);          // 现在
        engine.addPoint("GOOG", now - 60000, 2850.20);
        engine.addPoint("GOOG", now, 2865.75);

        std::cout << " 添加了 4 个新数据点\n";
        std::cout << "   当前总点数: " << engine.totalPoints() << "\n";
        printSeparator();

        // ========== 4. 执行查询 ==========
        std::cout << " 执行各种查询...\n\n";

        // 获取所有股票代码
        auto stocks = engine.getAllStockCodes();

        for (const auto& stock : stocks) {
            std::cout << "股票: " << stock << "\n";
            std::cout << "  ├─ 数据点数: " << engine.pointCount(stock) << "\n";
            std::cout << "  ├─ 时间范围: "
                << formatTime(engine.minTimestamp(stock)) << " → "
                << formatTime(engine.maxTimestamp(stock)) << "\n";
            std::cout << "  ├─ 价格范围: $"
                << std::fixed << std::setprecision(2)
                << engine.minPrice(stock) << " → $"
                << engine.maxPrice(stock) << "\n";

            // 查询最近5个点
            auto end_time = engine.maxTimestamp(stock);
            auto start_time = end_time - 5 * 60000;  // 5分钟范围

            auto prices = engine.queryRange(stock, start_time, end_time);

            std::cout << "  └─ 最近 " << prices.size() << " 个价格: ";
            for (double p : prices) {
                std::cout << "$" << p << " ";
            }
            std::cout << "\n\n";
        }

        printSeparator();

        // ========== 5. 时间点查询 ==========
        std::cout << "  精确时间点查询:\n\n";

        if (!stocks.empty()) {
            std::string test_stock = stocks[0];
            int64_t mid_time = (engine.minTimestamp(test_stock) +
                engine.maxTimestamp(test_stock)) / 2;

            double price = engine.getPriceAt(test_stock, mid_time);

            std::cout << "  股票: " << test_stock << "\n";
            std::cout << "  时间: " << formatTime(mid_time) << "\n";
            std::cout << "  价格: $" << std::fixed << std::setprecision(2) << price << "\n";

            // 测试不存在的点
            double not_found = engine.getPriceAt(test_stock, 1);
            std::cout << "  不存在的点: " << not_found << " (返回0)\n";
        }

        printSeparator();

        // ========== 6. 多股票查询 ==========
        std::cout << " 多股票并行查询:\n\n";

        if (stocks.size() >= 2) {
            std::vector<std::string> query_stocks = { stocks[0], stocks[1] };

            auto results = engine.queryMultiRange(query_stocks,
                engine.minTimestamp(stocks[0]),
                engine.maxTimestamp(stocks[0]));

            for (const auto& [code, prices] : results) {
                std::cout << "  " << code << ": " << prices.size() << " 个点\n";
            }
        }

        printSeparator();

        // ========== 7. 保存和加载 ==========
        std::cout << " 测试持久化功能...\n";

        std::string backup_file = "stock_db_backup.dat";

        if (engine.saveToFile(backup_file)) {
            std::cout << " 数据保存到: " << backup_file << "\n";

            // 创建新引擎并加载
            StorageEngine engine2("restored_db");
            engine2.initialize();

            if (engine2.loadFromCSV(backup_file)) {
                std::cout << " 成功从文件恢复数据\n";
                std::cout << "   恢复后点数: " << engine2.totalPoints() << "\n";
                std::cout << "   恢复后股票数: " << engine2.stockCount() << "\n";
            }
        }

        printSeparator();

        // ========== 8. CSV 导出 ==========
        std::cout << " 导出 AAPL 数据到 CSV:\n\n";

        if (engine.hasStock("AAPL")) {
            std::string aapl_csv = engine.toCSV("AAPL");

            // 只显示前几行
            std::cout << "AAPL 数据 (前5行):\n";
            std::istringstream iss(aapl_csv);
            std::string line;
            int lines = 0;

            while (std::getline(iss, line) && lines < 6) {
                std::cout << "  " << line << "\n";
                lines++;
            }
        }

        printSeparator();

        // ========== 9. 性能测试 ==========
        std::cout << " 简单性能测试:\n\n";

        auto start = std::chrono::high_resolution_clock::now();

        // 执行1000次随机查询
        int queries = 1000;
        for (int i = 0; i < queries; i++) {
            if (!stocks.empty()) {
                engine.queryRange(stocks[i % stocks.size()],
                    engine.minTimestamp(stocks[0]),
                    engine.maxTimestamp(stocks[0]));
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "  " << queries << " 次范围查询耗时: " << ms.count() << " ms\n";
        std::cout << "  平均每次: " << (ms.count() * 1000.0 / queries) << " μs\n";

        printSeparator();

        // ========== 10. 内存统计 ==========
        std::cout << " 内存使用统计:\n\n";
        std::cout << "  总点数: " << engine.totalPoints() << "\n";
        std::cout << "  内存占用: " << engine.memoryUsage() / 1024.0 << " KB\n";
        std::cout << "  每点平均: "
            << (engine.memoryUsage() * 1.0 / engine.totalPoints()) << " 字节\n";

        printSeparator();

        // ========== 完成 ==========
        std::cout << " 演示完成！\n";
        std::cout << "\n感谢使用 StockDB-Lite!\n\n";

    }
    catch (const std::exception& e) {
        std::cerr << " 错误: " << e.what() << "\n";
        return 1;
    }

    return 0;
}