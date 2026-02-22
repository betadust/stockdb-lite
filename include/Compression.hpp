/**
 * @file include/Compression.hpp
 * @brief 压缩状态枚举类和压缩块
 * @author: beta dust
 * @date: [2026-02-20]
 */

#pragma once

#include <cstdint>

namespace high_frequency_storage {

    /**
     * @brief 压缩算法类型
     */
    enum class CompressionType {
        NONE,           // 未压缩
        DIFF_TIMESTAMP, // 时间戳差分编码
        DIFF_PRICE,     // 价格差分编码
        DIFF_BOTH       // 两者都压缩
    };

    /**
     * @brief 压缩块的元信息
     */
    struct CompressedBlock {
        int64_t base_timestamp;      // 基准时间戳
        double base_price;           // 基准价格
        uint32_t count;              // 数据点数量
        uint32_t timestamp_bytes;    // 时间戳压缩数据字节数
        uint32_t price_bytes;        // 价格压缩数据字节数
        // 后面跟着压缩数据：
        // - 时间戳差值数组
        // - 价格差值数组
    };

} // namespace high_frequency_storage