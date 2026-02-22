/**
 * @file: include/Compressor.hpp
 * @brief: 压缩器类声明
 * @author: beta dust
 * @date [2026-02-20]
 */ 
#pragma once

#include "Compression.hpp"
#include "PricePoint.hpp"
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace high_frequency_storage {

    /**
     * @brief 差分编码压缩器
     *
     * 压缩格式说明：
     * - 时间戳压缩：[base_timestamp(8字节)] [delta1(varint)] [delta2(varint)] ...
     * - 价格压缩：[base_price(8字节)] [delta1(varint)] [delta2(varint)] ...
     *
     * base_timestamp: 第一个时间戳的原始值
     * delta_i: 第i个时间戳与第i-1个时间戳的差值（ZigZag编码+Varint编码）
     */
    class Compressor {
    public:
        static constexpr double PRICE_PRECISION = 100.0;  // 价格精度：2位小数
        static constexpr double PRICE_PRECISION_INV = 0.01;

        // ---------- 压缩接口 ----------

        static size_t compressTimestamps(const std::vector<int64_t>& timestamps,
            std::vector<uint8_t>& compressed);

        static size_t compressPrices(const std::vector<double>& prices,
            std::vector<uint8_t>& compressed);

        static size_t compressPoints(const std::vector<PricePoint>& points,
            CompressedBlock& block,
            std::vector<uint8_t>& compressed_data);

        // ---------- 解压接口 ----------

        static void decompressTimestamps(const std::vector<uint8_t>& compressed,
            std::vector<int64_t>& timestamps);

        static void decompressPrices(const std::vector<uint8_t>& compressed,
            std::vector<double>& prices);

        static void decompressPoints(const CompressedBlock& block,
            const std::vector<uint8_t>& compressed_data,
            std::vector<PricePoint>& points);

    private:
        // ---------- 编码工具 ----------

        static uint64_t zigzagEncode(int64_t value) {
            return (value << 1) ^ (value >> 63);
        }

        static int64_t zigzagDecode(uint64_t value) {
            return (value >> 1) ^ ( (value & 1) ? -1 : 0 );
        }

        static size_t varintEncode(uint64_t value, uint8_t* output) {
            size_t bytes = 0;
            do {
                uint8_t byte = value & 0x7F;
                value >>= 7;
                if (value) byte |= 0x80;
                output[bytes++] = byte;
            } while (value);
            return bytes;
        }

        static size_t varintDecode(const uint8_t* input, uint64_t& value) {
            value = 0;
            size_t bytes = 0;
            uint64_t shift = 0;
            while (true) {
                uint8_t byte = input[bytes++];
                value |= (uint64_t)(byte & 0x7F) << shift;
                if (!(byte & 0x80)) break;
                shift += 7;
            }
            return bytes;
        }

        // 价格转换工具
        
        /**
         * @brief 价格压缩精度说明
         *
         * 价格使用定点数方式压缩：
         * - 将 double 乘以 100 转换为整数（保留2位小数）
         * - 存储整数差值，而不是原始浮点数
         * - 解压时除以 100 恢复原值
         *
         * 精度损失：±0.005 以内，适合股票价格数据
         * 如果需要更高精度，可以调整 PRECISION 常量
         */
        static int64_t priceToDelta(double price) {
            return static_cast<int64_t>(price * PRICE_PRECISION + 0.5);
        }

        static double deltaToPrice(int64_t delta) {
            return static_cast<double>(delta) * PRICE_PRECISION_INV;
        }
    };

} // namespace high_frequency_storage