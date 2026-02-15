// 文件名: StockSeries.h
#pragma once
#include "PricePoint.hpp"
#include <vector>
#include <string>

class StockSeries {
private:
    std::string stock_code_;           // 股票代码
    std::vector<PricePoint> data_;      // 原始数据（阶段一用）
    // 后续会加：压缩后的数据、内存池句柄等
    
public:
    explicit StockSeries(const std::string& code);
    
    // 基础接口
    void addPrice(int64_t timestamp, double price);
    size_t size() const;
    
    // 查询接口（阶段一：线性扫描）
    std::vector<double> queryRange(int64_t start, int64_t end) const;
    
    // 后续会加：压缩接口、内存优化接口
};