/**
 * @file include/PricePoint.h
 * @author @betadust
 * @date [2026-02-15]
 */

#pragma once

#include <cstdint>  // for int64_t
#include <string>   
#include <compare>  // for operator<=> (C++20)

 /**
  * @brief 表示一个股票在某个时间点的价格信息
  *
  *
 */

class PricePoint {
public:
    //std::string symbol;   // 股票代码（暂不包含）
    int64_t timestamp;    // 时间戳，单位为毫秒
    double price;        // 价格 n

    // --- 构造函数与重载 ---
    PricePoint() : timestamp(0), price(0.0) {}
    PricePoint(int64_t ts, double p) : timestamp(ts), price(p) {}


    // 默认的三路比较运算符，比较所有成员变量
    auto operator<=>(const PricePoint& other) const = default;

    // --- 工具成员函数 ---

    // 将时间戳转换为可读的日期时间字符串
    std::string toString() const {
        // 格式化时间戳为日期时间字符串
		int year = timestamp / (365LL * 24 * 3600 * 1000) + 1970;
		int month = (timestamp / (30LL * 24 * 3600 * 1000)) % 12 + 1;
		int day = (timestamp / (24LL * 3600 * 1000)) % 30 + 1;
		int hour = (timestamp / (3600LL * 1000)) % 24;
		int minute = (timestamp / (60LL * 1000)) % 60;
		int second = (timestamp / 1000) % 60;
		int ms = timestamp % 1000;
        std::string dateTimeStr = std::to_string(year) + "-" + (month < 10 ? "0" : "") + std::to_string(month) + "-" +
            (day < 10 ? "0" : "") + std::to_string(day) + " " +
            (hour < 10 ? "0" : "") + std::to_string(hour) + ":" +
            (minute < 10 ? "0" : "") + std::to_string(minute) + ":" +
            (second < 10 ? "0" : "") + std::to_string(second) + "." +
			(ms < 100 ? (ms < 10 ? "00" : "0") : "") + std::to_string(ms);
       
        return "Timestamp: " + dateTimeStr + ", Price: " + std::to_string(price);
    }

    // 判断是否有效（例如，价格必须为正数）
    bool isValid() const {
        return price > 0.0 && timestamp > 0;
    }


};
