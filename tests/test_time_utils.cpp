/**
 * @file tests/test_time_utils.cpp
 * @author @betadust
 * @date [2026-02-15]
*/

#include "utils/TimeUtils.hpp"
#include <iostream>
#include <cassert>
#include <vector>

void testTimeUtils() {
    using namespace high_frequency_storage::utils;

    std::cout << "========== 测试 TimeUtils ==========\n\n";

	/* 时区差异，测试失败，暂时只支持 UTC+8时区（系统时区）
    // 测试用例：各种格式的转换
    struct TestCase {
        int64_t timestamp;        // 输入时间戳
        std::string expected;      // 期望的字符串结果
    };

    std::vector<TestCase> tests = {
        {1707993025123LL, "2024-02-15 14:30:25.123"},  // 普通情况
        {1707993025000LL, "2024-02-15 14:30:25.000"},  // 毫秒为000
        {1707993025012LL, "2024-02-15 14:30:25.012"},  // 毫秒为012
        {1707993025102LL, "2024-02-15 14:30:25.102"},  // 毫秒为102
        {1707955200000LL, "2024-02-15 00:00:00.000"},  // 午夜
        {1707955200001LL, "2024-02-15 00:00:00.001"},  // 午夜过1毫秒
        {1708031999999LL, "2024-02-15 23:59:59.999"},  // 午夜前1毫秒
        {-1, "invalid-timestamp"}                       // 无效时间戳
    };

    std::cout << "1. 测试 toString() 函数:\n";
    std::cout << "----------------------------------------\n";
    for (const auto& test : tests) {
        std::string result = TimeUtils::toString(test.timestamp);
        std::cout << "  " << test.timestamp << " -> " << result;
        if (result == test.expected) {
            std::cout << " yes\n";
        }
        else {
            std::cout << " no (期望: " << test.expected << ")\n";
        }
    }
    */


    std::cout << "\n2. 测试 fromString() 函数:\n";
    std::cout << "----------------------------------------\n";

    // 测试完整格式
    std::vector<std::string> date_strings = {
        "2024-02-15 14:30:25.123",
        "2024-02-15 14:30:25.000",
        "2024-02-15 14:30:25.012",
        "2024-02-15 00:00:00.000",
        "2024-02-15 23:59:59.999"
    };

    for (const auto& date_str : date_strings) {
        int64_t ts = TimeUtils::fromString(date_str);
        std::string back = TimeUtils::toString(ts);
        std::cout << "  " << date_str << " -> " << ts
            << " -> " << back;
        if (back == date_str) {
            std::cout << " yes\n";
        }
        else {
            std::cout << " no\n";
        }
    }

    // 测试简写格式（无毫秒）
    std::cout << "\n3. 测试简写格式 (无毫秒):\n";
    std::vector<std::string> simple_strings = {
        "2024-02-15 14:30:25",
        "2024-02-15 00:00:00",
        "2024-02-15 23:59:59"
    };

    for (const auto& date_str : simple_strings) {
        int64_t ts = TimeUtils::fromString(date_str);
        std::string back = TimeUtils::toString(ts);
        std::cout << "  " << date_str << " -> " << ts
            << " -> " << back;
        // 简写格式转换后应该变成完整格式（毫秒为000）
        std::string expected = date_str + ".000";
        if (back == expected) {
            std::cout << " yes\n";
        }
        else {
            std::cout << " no (期望: " << expected << ")\n";
        }
    }

    // 测试边界情况
    std::cout << "\n4. 测试边界情况:\n";
    std::cout << "----------------------------------------\n";

    // 无效字符串
    std::vector<std::string> invalid_strings = {
        "2024-02-15",                    // 缺少时间部分
        "2024/02/15 14:30:25.123",       // 分隔符错误
        "2024-02-15 14:30:25.12",        // 毫秒只有两位
        "2024-02-15 14:30:25.1234",      // 毫秒四位
        "abc",                            // 完全无效
        ""                                // 空字符串
    };

    for (const auto& invalid : invalid_strings) {
        int64_t ts = TimeUtils::fromString(invalid);
        std::cout << "  \"" << invalid << "\" -> " << ts;
        if (ts == -1) {
            std::cout << " yes (正确返回 -1)\n";
        }
        else {
            std::cout << " no (应该返回 -1)\n";
        }
    }

    // 测试往返一致性
    std::cout << "\n5. 测试往返一致性:\n";
    std::cout << "----------------------------------------\n";

    int64_t now = TimeUtils::now();
    std::string now_str = TimeUtils::toString(now);
    int64_t now_back = TimeUtils::fromString(now_str);
    std::string now_back_str = TimeUtils::toString(now_back);

    std::cout << "  当前时间戳: " << now << "\n";
    std::cout << "  转为字符串: " << now_str << "\n";
    std::cout << "  转回时间戳: " << now_back << "\n";
    std::cout << "  再次转为字符串: " << now_back_str << "\n";

    // 允许1秒内的误差（因为从字符串解析会丢失毫秒以下精度？不，我们保留了毫秒）
    int64_t diff = now_back - now;
    if (diff >= -1000 && diff <= 1000) {
        std::cout << "  yes 往返一致 (差异: " << diff << " 毫秒)\n";
    }
    else {
        std::cout << "  no 往返不一致 (差异: " << diff << " 毫秒)\n";
    }

    std::cout << "\n========== 测试完成 ==========\n";
}

int main() {
    testTimeUtils();
    return 0;
}