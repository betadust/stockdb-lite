/**
* @file test/test_time_utils_thread.cpp
* @author @betadust
* @date [2026-02-15]
*/


#include "utils/TimeUtils.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>

using namespace high_frequency_storage::utils;

std::mutex cout_mutex;

void testThreadSafety() {
    std::cout << "\n========== 线程安全测试 ==========\n";

    std::vector<std::thread> threads;
    std::atomic<bool> failed{ false };
    std::atomic<int> success_count{ 0 };
    std::atomic<int> total_tests{ 0 };

    // 生成多样化的测试数据
    std::vector<int64_t> test_timestamps;

    // 1. 边界值
    std::cout << "生成测试数据...\n";
    test_timestamps.push_back(0);                      // 1970-01-01
    test_timestamps.push_back(1000000000000LL);        // 2001-09-09
    test_timestamps.push_back(1577836800000LL);        // 2020-01-01
    test_timestamps.push_back(1609459200000LL);        // 2021-01-01
    test_timestamps.push_back(1640995200000LL);        // 2022-01-01
    test_timestamps.push_back(1672531200000LL);        // 2023-01-01
    test_timestamps.push_back(1704067200000LL);        // 2024-01-01
    test_timestamps.push_back(1735689600000LL);        // 2025-01-01
    test_timestamps.push_back(1767225600000LL);        // 2026-01-01
    test_timestamps.push_back(1893456000000LL);        // 2030-01-01

    // 2. 特殊时间点
    test_timestamps.push_back(1707984000000LL);        // 2024-02-15 00:00:00.000
    test_timestamps.push_back(1707993025123LL);        // 2024-02-15 14:30:25.123
    test_timestamps.push_back(1708031999999LL);        // 2024-02-15 23:59:59.999

    // 3. 毫秒边界
    test_timestamps.push_back(1707993025000LL);        // .000
    test_timestamps.push_back(1707993025001LL);        // .001
    test_timestamps.push_back(1707993025010LL);        // .010
    test_timestamps.push_back(1707993025100LL);        // .100

    // 4. 随机时间戳
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dist(1577836800000LL, 1893456000000LL);

    for (int i = 0; i < 100; i++) {
        test_timestamps.push_back(dist(gen));
    }

    std::cout << "测试数据生成完成，共 " << test_timestamps.size() << " 个时间点\n";
    std::cout << "启动 10 个线程，每个线程测试 1000 次...\n\n";

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&test_timestamps, &failed, &success_count, &total_tests, i]() {
            int tests_per_thread = 1000;

            for (int j = 0; j < tests_per_thread; j++) {
                total_tests++;

                // 轮流使用不同的时间戳
                size_t idx = (j + i * 37) % test_timestamps.size();
                int64_t original_ts = test_timestamps[idx];

                // 测试1：toString -> fromString 往返
                std::string str = TimeUtils::toString(original_ts);
                int64_t recovered_ts = TimeUtils::fromString(str);

                int64_t diff = recovered_ts - original_ts;
                if (diff < -1 || diff > 1) {
                    failed = true;
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "\n 线程 " << i << " 错误:\n"
                        << "  原始时间戳: " << original_ts << "\n"
                        << "  字符串: " << str << "\n"
                        << "  恢复时间戳: " << recovered_ts << "\n"
                        << "  差异: " << diff << " ms\n";
                    return;
                }

                // 测试2：字符串解析一致性
                if (!failed && j % 10 == 0) {
                    std::string test_str = "2024-02-15 14:30:25.123";
                    int64_t ts_from_str = TimeUtils::fromString(test_str);
                    std::string back_str = TimeUtils::toString(ts_from_str);

                    if (back_str != test_str) {
                        failed = true;
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cout << "\n 线程 " << i << " 字符串解析错误:\n"
                            << "  原始字符串: " << test_str << "\n"
                            << "  解析为: " << ts_from_str << "\n"
                            << "  转回字符串: " << back_str << "\n";
                        return;
                    }
                }

                // 测试3：当前时间
                if (!failed && j % 50 == 0) {
                    int64_t now = TimeUtils::now();
                    std::string now_str = TimeUtils::toString(now);
                    int64_t now_back = TimeUtils::fromString(now_str);

                    if (std::abs(now_back - now) > 1) {
                        failed = true;
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cout << "\n 线程 " << i << " 当前时间错误:\n"
                            << "  当前时间戳: " << now << "\n"
                            << "  字符串: " << now_str << "\n"
                            << "  恢复时间戳: " << now_back << "\n"
                            << "  差异: " << (now_back - now) << " ms\n";
                        return;
                    }
                }

                success_count++;

                // 进度输出
                if (j % 200 == 0 && j > 0) {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "线程 " << i << " 进度: " << j << "/" << tests_per_thread << "\n";
                }
            }

            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "线程 " << i << " 完成\n";
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\n========== 测试结果 ==========\n";
    std::cout << "  总测试次数: " << total_tests << "\n";
    std::cout << "  成功次数: " << success_count << "\n";
    std::cout << "  耗时: " << elapsed.count() << " ms\n";

    if (!failed) {
        std::cout << "   线程安全测试通过！\n";
        std::cout << "   所有时间戳转换正确\n";
        std::cout << "   无数据竞争和崩溃\n";
    }
    else {
        std::cout << "   线程安全测试失败\n";
    }
    std::cout << "================================\n";
}

int main() {
    testThreadSafety();
    return 0;
}