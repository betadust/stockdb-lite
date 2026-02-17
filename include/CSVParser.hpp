/**
 * @file include/CSVParser.hpp 
 * @author betadust
 * @date [2026-02-17]
*/

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace high_frequency_storage {

    /**
     * @brief CSV 文件解析器
     *
     * 提供静态方法解析 CSV 格式的数据，支持：
     * - 自定义分隔符（默认逗号）
     * - 处理引号包裹的字段
     * - 跳过空行
     * - 解析文件或字符串
     */
    class CSVParser {
    public:
        // ---------- 基础解析接口 ----------

        /**
         * @brief 解析一行 CSV 数据
         * @param line 一行 CSV 字符串
         * @param delimiter 字段分隔符，默认为 ','
         * @return 解析后的字段列表
         *
         * 示例：
         *   parseLine("AAPL,2024-02-15,175.23") -> ["AAPL", "2024-02-15", "175.23"]
         *   parseLine("AAPL,\"2024-02-15\",175.23") -> ["AAPL", "2024-02-15", "175.23"]
         * 
         * @note 线程安全说明：
         *   - 此函数是线程安全的，可以并发调用
         *   - 但传入的 line 字符串如果在外部被修改，会导致数据竞争
         *   - 调用者需确保在多线程环境中，line 不会被其他线程修改
         */
        static std::vector<std::string> parseLine(const std::string& line,
            char delimiter = ',');

        /**
         * @brief 解析整个 CSV 字符串
         * @param csv_data 多行 CSV 数据
         * @param delimiter 字段分隔符
         * @param skip_empty_lines 是否跳过空行
         * @return 二维数组，每行是一个字段列表
         */
        static std::vector<std::vector<std::string>> parseString(
            const std::string& csv_data,
            char delimiter = ',',
            bool skip_empty_lines = true);

        /**
         * @brief 从文件解析 CSV
         * @param filename 文件名
         * @param delimiter 字段分隔符
         * @param skip_empty_lines 是否跳过空行
         * @return 二维数组，每行是一个字段列表
         * @throws std::runtime_error 如果文件无法打开
         */
        static std::vector<std::vector<std::string>> parseFile(
            const std::string& filename,
            char delimiter = ',',
            bool skip_empty_lines = true);

        // ---------- 带类型转换的解析接口 ----------

        /**
         * @brief 解析并转换为指定类型
         * @tparam T 目标类型（必须支持从 string 转换）
         * @param line 一行 CSV 字符串
         * @param delimiter 字段分隔符
         * @return 转换后的字段列表
         * @throws std::invalid_argument 如果转换失败
         */
        template<typename T>
        static std::vector<T> parseLineAs(const std::string& line,
            char delimiter = ',');

        /**
         * @brief 解析整文件并转换为指定类型
         * @tparam T 目标类型
         * @param filename 文件名
         * @param delimiter 字段分隔符
         * @return 二维数组，每行是转换后的字段列表
         */
        template<typename T>
        static std::vector<std::vector<T>> parseFileAs(
            const std::string& filename,
            char delimiter = ',');

        // ---------- 流式解析接口（大文件）----------

        /**
         * @brief CSV 行迭代器（用于大文件流式处理）
         */
        class Iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = std::vector<std::string>;
            using difference_type = std::ptrdiff_t;
            using pointer = const value_type*;
            using reference = const value_type&;

			// ---------- 构造与迭代器接口 ----------
            
            //结束迭代器
            Iterator() = default;
            //开始迭代器
            explicit Iterator(const std::string& filename, char delimiter = ',');

            // ---------- （禁止）拷贝语义 ---------- 
            Iterator(const Iterator& other) = delete;
			Iterator & operator=(const Iterator & other) = delete;
            // ---------- 移动语义 ----------
            Iterator(Iterator&& other) noexcept = default;
            Iterator& operator=(Iterator&& other) noexcept = default;
            // ---------- 核心迭代器操作 (内联实现)----------
            Iterator& operator++() {
				readNext();
				return *this;
            }
            Iterator operator++(int) {
				Iterator tmp = std::move(*this); // 注意：这是移动，不是拷贝
				readNext();
                return tmp;
            }
            reference operator*() const {
                if (!is_valid_) {
                    throw std::out_of_range("Dereferencing invalid iterator");
                }
                return current_row_;
            }
            pointer operator->() const {
                if (!is_valid_) {
                    throw std::out_of_range("Dereferencing invalid iterator");
                }
                return &current_row_;
            }
            // ---------- 比较操作（内联） ----------
            bool operator==(const Iterator& other) const {
                if (!is_valid_ && !other.is_valid_) return true;  // 都是结束
                return false;
            }
            bool operator!=(const Iterator& other) const {
                return !(*this == other);
            }
            // ---------- 获取当前状态 ----------
            size_t rowNumber() const { return row_number_; }
            bool isValid() const { return is_valid_; }
            void reset();
        private:
            std::unique_ptr<std::ifstream> file_;  // 独占文件流
            char delimiter_ = ',';
            value_type current_row_;
            size_t row_number_ = 0;
            bool is_valid_ = false;

            void readNext();
        };


        // ---------- 工具函数 ----------

        /**
         * @brief 判断字符串是否只包含空白字符
         */
        static bool isWhitespace(const std::string& str);

        /**
         * @brief 去除字符串首尾的空白字符
         */
        static std::string trim(const std::string& str);

        /**
         * @brief 去除字段两端的引号
         * @param field 原始字段
         * @return 去除引号后的字符串
         */
        static std::string unquote(const std::string& field);

        /**
         * @brief 转义 CSV 字段（为写入做准备）
         * @param field 原始字段
         * @param delimiter 分隔符
         * @return 转义后的字段（如果需要加引号）
         */
        static std::string escape(const std::string& field, char delimiter = ',');

    private:
        // ---------- 内部实现辅助函数 ----------

        /**
         * @brief 从pos位置解析引号包括住的字段
         * @param line 当前行
         * @param pos 当前解析位置（引用传递）
         * @param delimiter 分隔符
         * @return 解析出的字段
		 * 注意CSV规范：引号内可以包含分隔符和换行符，双引号表示转义引号
		 * 例 "123""456" -> 123"456
		 *    """123""" -> "123"
         */
        static std::string parseQuotedField(const std::string& line,
            size_t& pos,
            char delimiter);

        /**
         * @brief 从pos位置解析普通字段（未被引号包括住，无""转义）
         */
        static std::string parseSimpleField(const std::string& line,
            size_t& pos,
            char delimiter);
    };

    // ---------- 工厂函数 ----------

    // 在同一个命名空间中定义工厂函数

    /**
     * @brief 为文件名创建开始迭代器
     */
    inline CSVParser::Iterator begin(const std::string& filename, char delimiter = ',') {
        return CSVParser::Iterator(filename, delimiter);
    }

    /**
     * @brief 创建结束迭代器
     */
    inline CSVParser::Iterator end() {
        return CSVParser::Iterator();
    }

    // 范围 for 循环支持
    inline CSVParser::Iterator begin(CSVParser::Iterator& it) {
        return std::move(it);  // 注意：这是移动，不是拷贝
    }
} // namespace high_frequency_storage

// 模板实现需要放在头文件中
#include "CSVParser.ipp"  // 如果模板实现较多，可以分离到 .tpp 文件