/**
 * @file tests/test_csv_parser.cpp
 * @brief CSVParser 测试套件
 * @author betadust
 * @date [2026-02-16]
 */ 
#include "CSVParser.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

void testBasicParsing() {
    using namespace high_frequency_storage;

    std::cout << "1. 基础解析测试\n";

    // 简单行
    auto fields1 = CSVParser::parseLine("AAPL,2024-02-15,175.23");
    for (auto s : fields1) {
		std::cout << s << "\n";
    }
    assert(fields1.size() == 3);
    assert(fields1[0] == "AAPL");
    assert(fields1[1] == "2024-02-15");
    assert(fields1[2] == "175.23");

    // 带引号的行
    auto fields2 = CSVParser::parseLine("\"Apple, Inc.\",\"2024-02-15\",175.23");
    assert(fields2.size() == 3);
    assert(fields2[0] == "Apple, Inc.");

    // 空字段
    auto fields3 = CSVParser::parseLine("AAPL,,175.23");
    assert(fields3.size() == 3);
    assert(fields3[1].empty());

    std::cout << "  基础解析通过\n";
}

void testFileParsing() {
    using namespace high_frequency_storage;

    std::cout << "2. 文件解析测试\n";

    // 创建测试文件
    std::ofstream test_file("test.csv");
    test_file << "stock_code,timestamp,price\n";
    test_file << "AAPL,2024-02-15,175.23\n";
    test_file << "GOOG,2024-02-15,2780.50\n";
    test_file << "\n";  // 空行
    test_file << "MSFT,2024-02-15,330.45\n";
    test_file.close();

    // 解析文件
    auto rows = CSVParser::parseFile("test.csv", ',', true);

    assert(rows.size() == 4);  // 4行数据（表头 + 3行数据，空行被跳过）
    assert(rows[0].size() == 3);
    assert(rows[0][0] == "stock_code");

    std::cout << "  文件解析通过\n";

    // 清理
    std::remove("test.csv");
}

void testTypeConversion() {
    using namespace high_frequency_storage;

    std::cout << "3. 类型转换测试\n";

    auto ints = CSVParser::parseLineAs<int>("1,2,3,4,5");
    assert(ints.size() == 5);
    assert(ints[0] == 1);
    assert(ints[4] == 5);

    auto doubles = CSVParser::parseLineAs<double>("1.1,2.2,3.3");
    assert(doubles.size() == 3);
    assert(doubles[1] == 2.2);

    std::cout << "   类型转换通过\n";
}

void testIterator() {
    using namespace high_frequency_storage;

    std::cout << "4. 迭代器测试\n";

    // 创建测试文件
    std::ofstream test_file("large.csv");
    for (int i = 0; i < 1000; i++) {
        test_file << "AAPL,2024-02-15," << (175.23 + i) << "\n";
    }
    test_file.close();

    // 使用迭代器逐行读取
    //std::ifstream file("large.csv");
    auto it = begin("large.csv", ',');
    auto ed = end();

    int count = 0;
    for (; it != ed; ++it) {
        auto row = *it;
        assert(row.size() == 3);
        count++;
    }

    assert(count == 1000);

    std::cout << "   迭代器测试通过\n";

    // 清理
    std::remove("large.csv");
}

int main() {
    std::cout << "========== CSVParser 测试 ==========\n\n";

    testBasicParsing();
    testFileParsing();
    testTypeConversion();
    testIterator();

    std::cout << "\n========== 所有测试通过 ==========\n";
    return 0;
}