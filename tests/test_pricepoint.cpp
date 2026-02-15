#include "PricePoint.hpp"
#include <iostream>
#include <vector>
#include <cassert>
using namespace high_frequency_storage;


int main() {
    // 测试默认构造
    PricePoint p1; 
    std::cout << "p1: " << p1.toString() << std::endl;

    // 测试带参构造
    PricePoint p2(20250214, 175.23);
    std::cout << "p2: " << p2.toString() << std::endl;

    // 测试有效性
    std::cout << "p2 is " << (p2.isValid() ? "valid" : "invalid") << std::endl;

    // 测试比较
    PricePoint p3(20250214, 176.45);
    std::cout << "p2 < p3: " << (p2 < p3) << std::endl;  // 按timestamp比，相等，再按price比

    // 测试放入vector
    std::vector<PricePoint> vec;
    vec.emplace_back(p2);
    vec.emplace_back(p3);
    std::cout << "vector size: " << vec.size() << std::endl;

    // 测试是否可平凡复制
    std::cout << "6. 验证平凡可复制:\n";
    std::cout << "  std::is_trivially_copyable_v<PricePoint> = "
        << std::is_trivially_copyable_v<PricePoint> << "\n";
    assert(std::is_trivially_copyable_v<PricePoint>);
    std::cout << "  平凡可复制通过\n\n";
    return 0;
}