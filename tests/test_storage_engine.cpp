
/** 
 * @file tests/test_storage_engine.cpp
 * @brief StorageEngine 测试套件
 * @author betadust
 * @date [2026-02-16]
*/
// tests/test_storage_engine.cpp


#include "StorageEngine.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace high_frequency_storage;

void testStorageEngine() {
    std::cout << "========== 测试 StorageEngine ==========\n\n";

    // 1. 创建和初始化
    std::cout << "1. 创建引擎...\n";
    StorageEngine engine("test_engine");
    engine.initialize(1000);
    assert(engine.isInitialized());
    assert(engine.name() == "test_engine");
    std::cout << " 创建成功\n\n";

    // 2. 添加数据
    std::cout << "2. 添加数据...\n";
    engine.addPoint("AAPL", 1707993025123LL, 175.23);
    engine.addPoint("AAPL", 1707993025124LL, 176.45);
    engine.addPoint("GOOG", 1707993025123LL, 2780.50);
    engine.addPoint("GOOG", 1707993025124LL, 2790.75);

    assert(engine.stockCount() == 2);
    assert(engine.totalPoints() == 4);
    std::cout << " 添加成功\n\n";

    // 3. 查询数据
    std::cout << "3. 查询数据...\n";
    auto aapl_prices = engine.queryRange("AAPL", 1707993025123LL, 1707993025124LL);
    assert(aapl_prices.size() == 2);
    assert(aapl_prices[0] == 175.23);

    auto price = engine.getPriceAt("AAPL", 1707993025124LL);
    assert(price == 176.45);
    std::cout << " 查询成功\n\n";

    // 4. 多股票查询
    std::cout << "4. 多股票查询...\n";
    std::vector<std::string> stocks = { "AAPL", "GOOG", "MSFT" };
    auto multi_results = engine.queryMultiRange(stocks, 1707993025123LL, 1707993025124LL);
    assert(multi_results["AAPL"].size() == 2);
    assert(multi_results["GOOG"].size() == 2);
    assert(multi_results["MSFT"].empty());
    std::cout << " 多股票查询成功\n\n";

    // 5. 统计信息
    std::cout << "5. 统计信息...\n";
    assert(engine.hasStock("AAPL"));
    assert(!engine.hasStock("MSFT"));
    assert(engine.pointCount("AAPL") == 2);
    assert(engine.minPrice("AAPL") == 175.23);
    assert(engine.maxPrice("GOOG") == 2790.75);
    std::cout << " 统计信息正确\n\n";

    // 6. 打印状态
    std::cout << "6. 引擎状态:\n";
    engine.printStatus();

    // 7. 数据维护
    std::cout << "\n7. 数据维护...\n";
    assert(engine.removeStock("GOOG"));
    assert(engine.stockCount() == 1);
    assert(engine.totalPoints() == 2);
    std::cout << " 删除成功\n\n";

    // 8. 异常处理
    std::cout << "8. 异常处理...\n";
    try {
        engine.queryRange("MSFT", 0, 100);
        assert(false);
    }
    catch (const std::out_of_range& e) {
        std::cout << " 捕获异常: " << e.what() << "\n";
    }

    std::cout << "\n========== 所有测试通过 ==========\n";
}

int main() {
    testStorageEngine();
    return 0;
}