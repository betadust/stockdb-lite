/**
 * @file include/PricePoint.hpp
 * @author @betadust
 * @date [2026-02-15]
 */

#pragma once

#include <cstdint>  // for int64_t
#include <string>   
#include <compare>  // for operator<=> (C++20)
#include "utils/TimeUtils.hpp"

 /**
  * @brief 表示一个股票在某个时间点的价格信息
  *
  *
 */

namespace high_frequency_storage{

    class PricePoint {
    private:
		// 目前不包含股票代码，因为 PricePoint 是 StockSeries 内部使用的结构，已经隐含了股票代码。
        //std::string symbol;   // 股票代码（暂不包含）
        int64_t timestamp_;    // 时间戳，单位为毫秒
        double price_;        // 价格 n
    public:


        // --- 构造函数与重载 ---
        PricePoint() : timestamp_(0), price_(0.0) {}
        PricePoint(int64_t ts, double p) : timestamp_(ts), price_(p) {}


        // 默认的三路比较运算符，比较所有成员变量
        auto operator<=>(const PricePoint& other) const = default;


        // --- 工具成员函数 ---

        // 将时间戳转换为可读的日期时间字符串
        std::string toString() const {
            if (!isValid()) {
                return "invalid";
            }
            return "Timestamp: " + utils::TimeUtils::toString(timestamp_) +
                ", Price: " + std::to_string(price_);
        }

        // 判断是否有效（例如，价格必须为正数）
        bool isValid() const {
            return price_ > 0.0 && timestamp_ > 0;
        }
    };
    static_assert(std::is_trivially_copyable_v<PricePoint>,
        "PricePoint should be trivially copyable");
}
