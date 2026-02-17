/**
 * @file src/CSVParser.cpp
 * @author betadust
 * @date [2026-02-17]
 */

#include "CSVParser.hpp"
#include <cctype>
#include <algorithm>
#include <fstream>
#include <sstream>
#include<iostream>
#include<string>

namespace high_frequency_storage {

	// ---------- 基础解析接口 ----------
	// @brief 解析一行 CSV 数据
	std::vector<std::string> CSVParser::parseLine(const std::string& line, char delimiter) {
		std::vector<std::string> fields;
		size_t pos = 0;
		const size_t len = line.length();
		while (pos < len) {
			// 跳过开头的空白
			while (pos < len && std::isspace(line[pos])) {
				pos++;
			}
			if (pos >= len) break;

			// 判断字段是否以引号开头
			if (line[pos] == '"') {
				//std::cout << "引号开头位置 " << pos << "\n";
				fields.push_back(parseQuotedField(line, pos, delimiter));
			}
			else {
				fields.push_back(parseSimpleField(line, pos, delimiter));
			}
			//std::cout << "本次解析: " << fields.back() << " 位置 " << pos << "\n";
			// 跳过字段后的空白和分隔符
			while (pos < len && (std::isspace(line[pos]) || line[pos] == delimiter)) {
				if (line[pos] == delimiter) {
					pos++;
					break;
				}
				pos++;
			}
		}
		return fields;
	}

	// @brief 解析整个 CSV 字符串 （多行）
	std::vector<std::vector<std::string>> CSVParser::parseString(const std::string& csv_data,
		char delimiter,
		bool skip_empty_lines) {

		std::vector<std::vector<std::string>> result;
		std::istringstream iss(csv_data);
		std::string line;

		while (std::getline(iss, line)) {
			// 处理行尾的回车符
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			// 跳过空行
			if (skip_empty_lines && isWhitespace(line)) {
				continue;
			}
			result.push_back(parseLine(line, delimiter));
		}
		return result;
	}
	
	// @brief 从文件解析 CSV
	std::vector<std::vector<std::string>> CSVParser::parseFile(const std::string& filename,
		char delimiter,
		bool skip_empty_lines) {
		
		std::ifstream file(filename);
		if (!file.is_open()) {
			throw std::runtime_error("无法打开文件: " + filename);
		}

		std::vector<std::vector<std::string>> result;
		std::string line;
		while (std::getline(file, line)) {
			// 处理行尾的回车符
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			//跳过空行
			if (skip_empty_lines && isWhitespace(line)) {
				continue;
			}
			result.push_back(parseLine(line, delimiter));
		}
		return result;
	}

	// ---------- 工具函数 ----------
	// 
	// @brief 判断字符串是否全由空白字符组成
	bool CSVParser::isWhitespace(const std::string& str) {
		//std::all_of 判断字符串是否全由空白字符组成
		return std::all_of(str.begin(), str.end(), [](unsigned char c) {
			return std::isspace(c);
		});
	}
	// @brief 去除字符串两端的空白字符
	std::string CSVParser::trim(const std::string& str) {
		auto start = str.begin();
		auto end = str.end();
		// 找到第一个非空白字符
		while (start != end && std::isspace(static_cast<unsigned char>(*start))) {
			++start;
		}
		// 找到最后一个非空白字符
		if (start != end) {
			do {
				--end;
			} while (std::distance(start, end) > 0 &&
				std::isspace(static_cast<unsigned char>(*end)));
			++end;  // 调整到最后一个非空白字符之后
		}
		return std::string(start, end);
	}
	// @brief 去除字段两端的引号
	std::string  CSVParser::unquote(const std::string& field) {
		if (field.size() < 2) return field;
		// 检查是否被引号包围
		if (field.front() == '"' && field.back() == '"') {
			std::string result = field.substr(1, field.size() - 2);
			// 处理转义的引号 ("")
			size_t pos = 0;
			while ((pos = result.find("\"\"", pos)) != std::string::npos) {
				result.replace(pos, 2, "\"");
				pos += 1;
			}

			return result;
		}

		return field;
	}
	// @brief 转义 CSV 字段（为写入做准备）
	std::string CSVParser::escape(const std::string& field, char delimiter) {
		bool needs_quotes = field.find(delimiter) != std::string::npos ||
			field.find('"') != std::string::npos ||
			field.find('\n') != std::string::npos ||
			field.find('\r') != std::string::npos;
		if (!needs_quotes) {
			return field;
		}
		std::string escaped = "\"";
		for (char c : field) {
			if (c == '"') {
				escaped += "\"\""; // 转义引号
			}
			else {
				escaped += c;
			}
		}
		escaped += "\"";
		return escaped;
	}

	// ---------- 内部实现辅助函数 ----------
	
	// @brief 从pos位置解析引号包括住的字段 （解析一个值）
	std::string CSVParser::parseQuotedField(const std::string& line, size_t& pos, char delimiter) {
		std::string field;
		pos++; // 跳过开头的引号
		while (pos < line.size()) {
			if (line[pos] == '"') {
				if (pos + 1 < line.size() && line[pos + 1] == '"') {
					field += '"'; // 转义引号
					pos += 2;
				}
				else {
					pos++; // 跳过结尾的引号
					break;
				}
			}
			else {
				field += line[pos++];
			}
		}
		// 跳过分隔符
		if (pos < line.size() && line[pos] == delimiter) {
			pos++;
		}
		return field;
	}

	// @brief 从pos位置解析普通字段（未被引号包括住，无""转义）
	std::string CSVParser::parseSimpleField(const std::string& line, size_t& pos, char delimiter) {
		std::string field;
		while (pos < line.length()) {
			if (line[pos] == delimiter) {
				break;
			}
			field += line[pos];
			pos++;
		}
		return trim(field); // 去除字段两端的空白
	}

	// ---------- 迭代器实现 ----------

	// @brief  Iterator begin() 参数构造函数实现
	CSVParser::Iterator::Iterator(const std::string& filename, char delimiter)
		: file_(std::make_unique<std::ifstream>(filename))
		, delimiter_(delimiter)
		, row_number_(0)
		, is_valid_(true) {

		if (!file_ || !file_->is_open()) {
			is_valid_ = false;
			throw std::runtime_error("Cannot open file: " + filename);
		}
		readNext();  // 读取第一行
	}



	// ---------- 获取当前状态 ----------

	// 重置到文件开头
	void CSVParser::Iterator::reset() {
		if (file_ && file_->is_open()) {
			file_->clear();
			file_->seekg(0, std::ios::beg);
			row_number_ = 0;
			readNext();
		}
	}

	// 读取下一行
	void CSVParser::Iterator::readNext() {
		if (!file_ || !file_->is_open()) {
			is_valid_ = false;
			return;
		}
		std::string line;
		if (std::getline(*file_, line)) {
			// 处理行尾的回车符
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			// 跳过空行
			if (CSVParser::isWhitespace(line)) {
				readNext();  // 递归读取下一行
				return;
			}
			current_row_ = CSVParser::parseLine(line, delimiter_);
			row_number_++;
			is_valid_ = true;
		}
		else {
			is_valid_ = false;
			current_row_.clear();
		}
	}


} // namespace high_frequency_storage