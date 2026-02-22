/**
 * @file: src/Compressor.cpp
 * @brief: 压缩器实现
 * @author: beta dust
 * @date: [2026-02-20]
 */ 

#include "Compressor.hpp"
#include <cstring>
#include <algorithm>
#include <limits>

namespace high_frequency_storage {

    // ========== 压缩实现 ==========
    size_t Compressor::compressTimestamps(const std::vector<int64_t>& timestamps,
        std::vector<uint8_t>& compressed) {
        if (timestamps.empty()) return 0;
        // 估算大小：8字节base + 每个差值平均2-3字节
        compressed.clear();
        compressed.reserve(8 + timestamps.size() * 3); //事先预约估计内存资源
        // 1. 存储基准时间戳
        int64_t base = timestamps[0];
        compressed.resize(sizeof(base));
        std::memcpy(compressed.data(), &base, sizeof(base));
        // 2. 计算并存储差值
        for (size_t i = 1; i < timestamps.size(); i++) {
            int64_t delta = timestamps[i] - timestamps[i - 1];
            uint64_t encoded = zigzagEncode(delta);
            uint8_t buffer[10];
            size_t bytes = varintEncode(encoded, buffer);
            size_t old_size = compressed.size();
            compressed.resize(old_size + bytes);
            std::memcpy(compressed.data() + old_size, buffer, bytes);
        }
        return compressed.size();
    }

    size_t Compressor::compressPrices(const std::vector<double>& prices,
        std::vector<uint8_t>& compressed) {
        if (prices.empty()) return 0;
        // 估算大小：8字节base + 每个差值平均2-3字节
        compressed.clear();
        compressed.reserve(8 + prices.size() * 3);
        // 1. 存储基准价格
        double base = prices[0]; //注意base存储的是double
        compressed.resize(sizeof(base));
        std::memcpy(compressed.data(), &base, sizeof(base));
        // 2. 转换为整数并计算差值
        int64_t last_int = static_cast<int64_t>(base * PRICE_PRECISION + 0.5);
        for (size_t i = 1; i < prices.size(); i++) {
            int64_t curr_int = static_cast<int64_t>(prices[i] * PRICE_PRECISION + 0.5);
            int64_t delta = curr_int - last_int;
            last_int = curr_int;
            uint64_t encoded = zigzagEncode(delta);
            uint8_t buffer[10];
            size_t bytes = varintEncode(encoded, buffer);
            size_t old_size = compressed.size();
            compressed.resize(old_size + bytes);
            std::memcpy(compressed.data() + old_size, buffer, bytes);
        }
        return compressed.size();
    }

    // ========== 解压实现 ==========

    void Compressor::decompressTimestamps(const std::vector<uint8_t>& compressed,
        std::vector<int64_t>& timestamps) {
        timestamps.clear();
        if (compressed.size() < 8) return;
        // 1. 读取基准时间戳
        int64_t base;
        std::memcpy(&base, compressed.data(), sizeof(base));
        timestamps.push_back(base);
        // 2. 解压差值
        const uint8_t* ptr = compressed.data() + sizeof(base);
        const uint8_t* end = compressed.data() + compressed.size();
        int64_t current = base;
        while (ptr < end) {
            uint64_t encoded;
            size_t bytes = varintDecode(ptr, encoded);
            ptr += bytes;
            int64_t delta = zigzagDecode(encoded);
            current += delta;
            timestamps.push_back(current);
        }
    }
    void Compressor::decompressPrices(const std::vector<uint8_t>& compressed,
        std::vector<double>& prices) {
        prices.clear();
        if (compressed.size() < 8) return;
        // 1. 读取基准价格
        double base;
        std::memcpy(&base, compressed.data(), sizeof(base));
        prices.push_back(base);
        // 2. 解压差值
        const uint8_t* ptr = compressed.data() + sizeof(base);
        const uint8_t* end = compressed.data() + compressed.size();
        int64_t current_int = static_cast<int64_t>(base * PRICE_PRECISION + 0.5);
        while (ptr < end) {
            uint64_t encoded;
            size_t bytes = varintDecode(ptr, encoded);
            ptr += bytes;
            int64_t delta = zigzagDecode(encoded);
            current_int += delta;
            double price = static_cast<double>(current_int) * PRICE_PRECISION_INV;
            prices.push_back(price);
        }
    }

    // ========== 批量压缩/解压 ==========

    size_t Compressor::compressPoints(const std::vector<PricePoint>& points,
        CompressedBlock& block,
        std::vector<uint8_t>& compressed_data) {
        if (points.empty()) return 0;
        // 提取数据
        std::vector<int64_t> timestamps;
        std::vector<double> prices;
        timestamps.reserve(points.size());
        prices.reserve(points.size());
        for (const auto& p : points) {
            timestamps.push_back(p.getTimestamp());
            prices.push_back(p.getPrice());
        }
        // 分别压缩
        std::vector<uint8_t> ts_compressed;
        std::vector<uint8_t> price_compressed; 
        compressTimestamps(timestamps,ts_compressed);
        compressPrices(prices,price_compressed);
        // 填充块信息
        block.base_timestamp = timestamps[0];
        block.base_price = prices[0];
        block.count = points.size();
        block.timestamp_bytes = ts_compressed.size();
        block.price_bytes = price_compressed.size();
        // 合并数据
        compressed_data.clear();
        compressed_data.reserve(ts_compressed.size() + price_compressed.size());
        compressed_data.insert(compressed_data.end(),
            ts_compressed.begin(), ts_compressed.end());
        compressed_data.insert(compressed_data.end(),
            price_compressed.begin(), price_compressed.end());
        return compressed_data.size();
    }

    void Compressor::decompressPoints(const CompressedBlock& block,
        const std::vector<uint8_t>& compressed_data,
        std::vector<PricePoint>& points) {
        // 分割压缩数据
        const uint8_t* ts_start = compressed_data.data();
        const uint8_t* price_start = ts_start + block.timestamp_bytes;
        std::vector<uint8_t> ts_compressed(ts_start, price_start);
        std::vector<uint8_t> price_compressed(price_start,
            compressed_data.data() + compressed_data.size());
        // 解压
        std::vector<int64_t> timestamps;
        std::vector<double> prices;
        decompressTimestamps(ts_compressed,timestamps);
        decompressPrices(price_compressed,prices);
        // 验证数量
        if (timestamps.size() != block.count || prices.size() != block.count) {
            throw std::runtime_error("Decompressed size mismatch");
        }
        // 合并
        points.clear();
        points.reserve(block.count);
        for (size_t i = 0; i < block.count; i++) {
            points.emplace_back(timestamps[i], prices[i]);
        }
    }
} // namespace high_frequency_storage