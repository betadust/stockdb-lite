
/**
 * @file src/StorageEngine.cpp
 * @brief 存储引擎实现
 * 
 * @author beta dust
 * @date [2026-02-17]
*/

// File: StorageEngine.cpp


#include "StorageEngine.hpp"
#include "CSVParser.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shared_mutex>
#include <iostream>

namespace high_frequency_storage {

	// @brief 构造函数
	StorageEngine::StorageEngine(const std::string& name) : 
		name_(name), total_points_(0), is_initialized_(false){
		// 设置默认配置
		config_.auto_sort = true;
		config_.reserve_size = 10000;
		config_.enable_stats = true;
	}

	// @brief 初始化引擎
	void StorageEngine::initialize(size_t reserve_size) {
		std::unique_lock lock(rw_mutex_);
		if (is_initialized_) {
			return; // 已经初始化，无需重复初始化
		}
		config_.reserve_size = reserve_size;
		series_map_.clear();
		total_points_ = 0;
		is_initialized_ = true;
	}
	
	// ---------- 数据导入接口 ----------

	// @brief 从 CSV 文件加载数据
	size_t StorageEngine::loadFromCSV(const std::string& filename, bool has_header) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			throw std::runtime_error("Cannot open file: " + filename);
		}
		auto it = begin(filename);
		auto ed = end();

		size_t count = 0;
		bool is_first_row = true;

		for (; it != ed; ++it) {
			auto row = *it;
			// 跳过表头
			if (is_first_row) {
				is_first_row = false;
				if (row.size() >= 1 && (row[0] == "stock_code" || row[0] == "code")) {
					continue;
				}
			}
			// 处理数据行...
			if (row.size() < 3) continue; // 不完整行，跳过
			try {
				std::string stock_code = row[0];
				// 解析时间戳
				int64_t timestamp;
				if (row[1].find('-') != std::string::npos) {
					timestamp = utils::TimeUtils::fromString(row[1]);
				}
				else {
					timestamp = std::stoll(row[1]);
				}
				double price = std::stod(row[2]);
				addPoint(stock_code, timestamp, price);
			}
			catch (const std::exception& e) {
				// 记录错误但继续处理
				std::cerr << "Error parsing row: " << e.what() << " - Skipping row." << std::endl;
				continue;
			}
			count++;
		}
		return count;
	}
	// @brief 从 CSV 字符串加载数据
	size_t StorageEngine::loadFromCSVString(const std::string& csv_data) {
		std::unique_lock lock(rw_mutex_);
		if (!is_initialized_) {
			initialize(config_.reserve_size);
		}
		// 使用 CSVParser 解析
		auto rows = CSVParser::parseString(csv_data, ',', true);
		size_t count = 0;
		bool is_first_row = true;
		for (const auto& row : rows) {
			// 跳过表头（如果第一行是列名）
			if (is_first_row) {
				is_first_row = false;
				if (row.size() >= 1 && (row[0] == "stock_code" || row[0] == "code")) {
					continue;
				}
			}
			if (row.size() < 3) continue; // 不完整行，跳过
			try {
				std::string stock_code = row[0];
				// 解析时间戳
				int64_t timestamp;
				if (row[1].find('-') != std::string::npos) {
					timestamp = utils::TimeUtils::fromString(row[1]);
				}
				else {
					timestamp = std::stoll(row[1]);
				}
				double price = std::stod(row[2]);
				addPoint(stock_code, timestamp, price);
				count++;
			}
			catch (const std::exception& e) {
				// 记录错误但继续处理
				std::cerr << "Error parsing row: " << e.what() << " - Skipping row." << std::endl;
				continue;
			}
		}
		return count;
	}
	

	// @brief 批量添加数据点
	void StorageEngine::addPoints(const std::string& stock_code, const std::vector<PricePoint>& points) {
		std::shared_lock lock(rw_mutex_);
		if (!is_initialized_) {
			initialize(config_.reserve_size);
		}
		// 查找或创建股票系列
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			auto series = std::make_unique<StockSeries>(stock_code);
			series->reserve(config_.reserve_size);
			it = series_map_.emplace(stock_code, std::move(series)).first; 
			//emplaec返回pair类型，first为迭代器，second为bool，表示是否插入成功
		}
		// 批量添加
		size_t before = it->second->size();
		it->second->addPrices(points);
		total_points_ += (it->second->size() - before);
	}
	
	// @brief 添加单个数据点 
	void StorageEngine::addPoint(const std::string& stock_code, int64_t timestamp, double price) {
		std::shared_lock lock(rw_mutex_);
		if (!is_initialized_) {
			initialize(config_.reserve_size);
		}
		// 查找或创建股票系列
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			auto series = std::make_unique<StockSeries>(stock_code);
			series->reserve(config_.reserve_size);
			it = series_map_.emplace(stock_code, std::move(series)).first; 
		}
		// 添加单个点
		size_t before = it->second->size();
		it->second->addPrice(timestamp, price);
		if (it->second->size() > before) total_points_++;
	}

	// ---------- 查询接口 ----------

	// @brief 查询指定股票在时间范围内的价格
	std::vector<double> StorageEngine::queryRange(const std::string& stock_code, int64_t start_time, int64_t end_time) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("股票代码 " + stock_code + " 不存在");
		}
		return it->second->queryRange(start_time, end_time);
	}

	// @brief 查询多个股票在时间范围内的价格
	std::unordered_map<std::string, std::vector<double>> StorageEngine::queryMultiRange(const std::vector<std::string>& stock_codes, int64_t start_time, int64_t end_time) const {
		std::shared_lock lock(rw_mutex_);
		std::unordered_map<std::string, std::vector<double>> result;
		for (const auto& code : stock_codes) {
			auto it = series_map_.find(code);
			if (it != series_map_.end()) {
				result[code] = it->second->queryRange(start_time, end_time);
			}
			else {
				result[code] = {}; // 股票不存在，返回空列表
			}
		}
		return result;
	}

	// @brief 获取某股票指定时间点的价格（最接近的）
	double StorageEngine::getPriceAt(const std::string& stock_code, int64_t timestamp) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("股票代码 " + stock_code + " 不存在");
		}
		return it->second->getPriceAt(timestamp);
	}

	// @brief 获取某股票的所有时间戳
	std::vector<int64_t> StorageEngine::getAllTimestamps(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("股票代码 " + stock_code + " 不存在");
		}
		std::vector<int64_t> res;
		for (auto tp : it->second->getMetadata()) {
			res.push_back(tp.getTimestamp());
		}
		return res;
	}

	// ---------- 统计信息接口 ----------

	std::vector<std::string> StorageEngine::getAllStockCodes() const {
		std::shared_lock lock(rw_mutex_);
		std::vector<std::string> codes;
		codes.reserve(series_map_.size());
		for (const auto& [code, _] : series_map_) {
			codes.push_back(code);
		}
		return codes;
	}

	size_t StorageEngine::pointCount(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("Stock not found: " + stock_code);
		}
		return it->second->size();
	}

	int64_t StorageEngine::minTimestamp(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("Stock not found: " + stock_code);
		}
		return it->second->minTimestamp();
	}

	int64_t StorageEngine::maxTimestamp(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("Stock not found: " + stock_code);
		}
		return it->second->maxTimestamp();
	}

	double StorageEngine::minPrice(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("Stock not found: " + stock_code);
		}
		return it->second->minPrice();
	}

	double StorageEngine::maxPrice(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			throw std::out_of_range("Stock not found: " + stock_code);
		}
		return it->second->maxPrice();
	}

	bool StorageEngine::hasStock(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		return series_map_.find(stock_code) != series_map_.end();
	}

	// ---------- 数据维护接口 ----------

	// @brief 清空所有数据
	void StorageEngine::clear() {
		std::unique_lock lock(rw_mutex_);
		series_map_.clear();
		total_points_ = 0;
		is_initialized_ = false;
	}

	// @brief 删除指定股票的数据
	bool StorageEngine::removeStock(const std::string& stock_code){
		std::unique_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			return false;
		}

		total_points_ -= it->second->size();
		series_map_.erase(it);
		return true;
	}

	// @brief 压缩指定股票的数据（阶段二预留接口）
	/*
	void StorageEngine::compress(const std::string& stock_code) {
		std::unique_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it != series_map_.end()) {
			// 后续实现压缩
			// it->second->compress();
		}
	}

	// @brief 压缩所有股票的数据（阶段二预留接口）
	void StorageEngine::compressAll() {
		std::unique_lock lock(rw_mutex_);

		for (auto& [_, series] : series_map_) {
			// 后续实现压缩
			// series->compress();
		}
	}
	*/

	// ---------- 序列化接口 ----------

	/*
	std::string StorageEngine::toCSV() const {
		std::shared_lock lock(rw_mutex_);

		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2);

		// 写入表头
		oss << "stock_code,timestamp,price\n";

		// 写入所有数据
		for (const auto& [code, series] : series_map_) {
			// 需要 StockSeries 提供导出接口
			// oss << series->toCSV();
			// 暂时简化
		}

		return oss.str();
	}

	std::string StorageEngine::toCSV(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);

		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			return "";
		}

		return it->second->toCSV();
	}

	bool StorageEngine::saveToFile(const std::string& filename) const {
		std::shared_lock lock(rw_mutex_);

		std::ofstream file(filename, std::ios::binary);
		if (!file.is_open()) {
			return false;
		}

		// 简单实现：保存为 CSV
		std::string csv = toCSV();
		file.write(csv.c_str(), csv.size());

		return file.good();
	}

	bool StorageEngine::loadFromFile(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			return false;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();

		loadFromCSVString(buffer.str());
		return true;
	}
	*/

	// ---------- 调试接口 ----------

	void StorageEngine::printStatus() const {
		std::shared_lock lock(rw_mutex_);

		std::cout << "========================================\n";
		std::cout << "Storage Engine: " << name_ << "\n";
		std::cout << "========================================\n";
		std::cout << "初始化状态: " << (is_initialized_ ? "已初始化" : "未初始化") << "\n";
		std::cout << "股票数量: " << series_map_.size() << "\n";
		std::cout << "总数据点: " << total_points_ << "\n";
		std::cout << "内存占用: " << memoryUsage() / 1024.0 / 1024.0 << " MB\n";
		std::cout << "配置:\n";
		std::cout << "  - 自动排序: " << (config_.auto_sort ? "是" : "否") << "\n";
		std::cout << "  - 预分配大小: " << config_.reserve_size << "\n";
		std::cout << "  - 启用统计: " << (config_.enable_stats ? "是" : "否") << "\n";

		if (!series_map_.empty()) {
			std::cout << "\n股票列表:\n";
			int count = 0;
			for (const auto& [code, series] : series_map_) {
				std::cout << "  - " << code << ": " << series->size() << " 点";
				if (++count >= 10) {
					std::cout << "  ... (共 " << series_map_.size() << " 只)";
					break;
				}
				std::cout << "\n";
			}
		}
		std::cout << "========================================\n";
	}

	void StorageEngine::printHead(const std::string& stock_code, size_t n) const {
		std::shared_lock lock(rw_mutex_);

		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			std::cout << "股票 " << stock_code << " 不存在\n";
			return;
		}

		it->second->printHead(n);
	}

	size_t StorageEngine::memoryUsage() const {
		std::shared_lock lock(rw_mutex_);

		size_t total = sizeof(*this);
		total += series_map_.bucket_count() * sizeof(void*);

		for (const auto& [code, series] : series_map_) {
			total += code.capacity();
			total += series->memoryUsage();
		}

		return total;
	}

} // namespace high_frequency_storage