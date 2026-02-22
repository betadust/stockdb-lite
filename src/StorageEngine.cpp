
/**
 * @file src/StorageEngine.cpp
 * @brief 存储引擎实现
 * 
 * @author beta dust
 * @date [2026-02-19]
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
				//std::cout << "Load From CSVString : " << timestamp << "\n";
				double price = std::stod(row[2]);
				addPointImpl(stock_code, timestamp, price);
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

	// @brief 从 CSV 文件加载数据
	size_t StorageEngine::loadFromCSV(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			throw std::runtime_error("Cannot open file: " + filename);
		}
		// 读取整个文件到字符串
		std::stringstream buffer;
		buffer << file.rdbuf();
		// 调用 loadFromCSVString 完成实际解析
		return loadFromCSVString(buffer.str());
	}

	// @brief 从大CSV文件加载数据（流式处理）
	size_t StorageEngine::loadLargeCSV(const std::string& filename) {
		std::unique_lock lock(rw_mutex_);
		if (!is_initialized_) {
			initialize(config_.reserve_size);
		}
		// 打开文件
		std::ifstream file(filename);
		if (!file.is_open()) {
			throw std::runtime_error("Cannot open file: " + filename);
		}
		// 获取文件大小（用于进度报告）
		file.seekg(0, std::ios::end);
		std::streampos file_size = file.tellg();
		file.seekg(0, std::ios::beg);
		file.close(); // 关闭后由 CSVParser::Iterator 重新打开
		// 使用 CSVParser 的迭代器逐行处理
		auto it = begin(filename);
		auto end_it = end();

		size_t count = 0;
		size_t line_number = 0;
		size_t error_count = 0;
		size_t report_interval = 100000;  // 每10万行报告一次
		// 简单判断表头
		if (it != end_it) {
			auto row = *it;
			// 若表头则跳过
			if (row.size() >= 1 && (row[0] == "stock_code" || row[0] == "code")) {
				++it;
				line_number++;
			}
		}
		// 记录开始时间
		auto start_time = std::chrono::steady_clock::now();
		auto last_report = start_time;
		for (; it != end_it; ++it, ++line_number) {
			const auto& row = *it;
			if (row.size() < 3) {
				error_count++;
				continue;
			}
			try {
				std::string stock_code = row[0];
				// 解析时间戳
				int64_t timestamp;
				if (row[1].find('-') != std::string::npos) {
					timestamp = utils::TimeUtils::fromString(row[1]);
					if (timestamp == -1) {
						error_count++;
						continue;
					}
				}
				else {
					timestamp = std::stoll(row[1]);
				}
				double price = std::stod(row[2]);
				// 验证数据
				if (!isValidPoint(stock_code, timestamp, price)) {
					error_count++;
					continue;
				}
				// 添加到批处理缓存
				// 批量缓存，可减少频繁的 addPoint 调用
				
				batch_cache_.add(stock_code, timestamp, price);
				count++;
				if (batch_cache_.needsFlush()) {
					batch_cache_.flushBatchCache(this);
				}
				// 定期报告进度
				if (line_number % report_interval == 0) {
					auto now = std::chrono::steady_clock::now();
					auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_report);
					if (elapsed.count() >= 5) {  // 至少5秒报告一次
						reportProgress(filename, line_number, count, error_count,
							file_size, file.tellg(), start_time);
						last_report = now;
					}
				}
			}
			catch (const std::exception& e) {
				error_count++;
				continue;
			}
		}
		// 刷新剩余的批处理缓存
		batch_cache_.flushBatchCache(this);
		// 最终报告
		auto end_time = std::chrono::steady_clock::now();
		auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
		std::cout << "\n=== 大文件加载完成 ===\n"
			<< "文件: " << filename << "\n"
			<< "总行数: " << line_number << "\n"
			<< "成功加载: " << count << " 点\n"
			<< "错误行数: " << error_count << "\n"
			<< "耗时: " << total_seconds.count() << " 秒\n"
			<< "速度: " << (count / std::max(1.0, double(total_seconds.count()))) << " 点/秒\n";
		return count;
	}

	// @brief 刷新批处理缓存
	void StorageEngine::BatchCache::flushBatchCache(StorageEngine* engine) {
		for (auto& [code, points] : cache_map_) {
			if (points.empty()) continue;
			auto it = engine->series_map_.find(code);
			if (it == engine->series_map_.end()) {
				auto series = std::make_unique<StockSeries>(code);
				series->reserve(engine->config_.reserve_size);
				it = engine->series_map_.emplace(code, std::move(series)).first;
			}
			// 批量添加
			it->second->addPrices(points);
			engine->total_points_ += points.size();
			points.clear();  // 清空以便复用
		}
		clear(); //	清空缓存
	}

	// 报告进度
	void StorageEngine::reportProgress(const std::string& filename,
		size_t line_number,
		size_t points_loaded,
		size_t error_count,
		std::streampos file_size,
		std::streampos current_pos,
		const std::chrono::steady_clock::time_point& start_time) {
		double progress = (file_size > 0) ?
			(double(current_pos) / double(file_size) * 100.0) : 0.0;
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
		double speed = (elapsed.count() > 0) ?
			(double(line_number) / elapsed.count()) : 0.0;
		// 估算剩余时间
		double remaining_seconds = (speed > 0 && progress < 99.0) ?
			(double(file_size - current_pos) / (speed * 1000)) : 0.0;
		int remaining_hours = int(remaining_seconds) / 3600;
		int remaining_minutes = (int(remaining_seconds) % 3600) / 60;
		int remaining_secs = int(remaining_seconds) % 60;
		std::cout << "\r进度: " << std::fixed << std::setprecision(1) << progress << "%"
			<< " | 行数: " << line_number
			<< " | 加载: " << points_loaded
			<< " | 错误: " << error_count
			<< " | 速度: " << int(speed) << " 行/秒"
			<< " | 剩余: ";
		if (remaining_hours > 0) {
			std::cout << remaining_hours << "h ";
		}
		if (remaining_minutes > 0 || remaining_hours > 0) {
			std::cout << remaining_minutes << "m ";
		}
		std::cout << remaining_secs << "s    " << std::flush;
	}

	// @brief 批量添加数据点
	void StorageEngine::addPoints(const std::string& stock_code, const std::vector<PricePoint>& points) {
		std::unique_lock lock(rw_mutex_);
		addPointsImpl(stock_code, points);
	}
	void StorageEngine::addPointsImpl(const std::string& stock_code, const std::vector<PricePoint>& points) {
		if (!isValidPoints(stock_code, points)) {
			throw std::invalid_argument("Invalid stock code or points");
		}
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
		std::unique_lock lock(rw_mutex_);
		addPointImpl(stock_code, timestamp, price);
	}
	void StorageEngine::addPointImpl(const std::string& stock_code, int64_t timestamp, double price) {
		if (!isValidPoint(stock_code, timestamp, price)) {
			throw std::invalid_argument("Invalid stock code, timestamp, or price");
		}
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

	// ---------- 序列化接口 ----------

	// @brief 将所有数据导出为 CSV 格式字符串
	std::string StorageEngine::toCSV() const {
		std::shared_lock lock(rw_mutex_);
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2);
		// 写入表头
		oss << "stock_code,timestamp,price\n";
		// 写入所有数据
		for (const auto& [code, series] : series_map_) {
			// 获取该股票的 CSV 数据（不包含表头）
			std::string series_csv = series->toCSV();
			// 直接追加到输出流
			oss << series_csv;
		}
		return oss.str();
	}

	// @brief 将指定股票的数据导出为 CSV 格式字符串
	std::string StorageEngine::toCSV(const std::string& stock_code) const {
		std::shared_lock lock(rw_mutex_);
		auto it = series_map_.find(stock_code);
		if (it == series_map_.end()) {
			return "";
		}
		return it->second->toCSV();
	}

	// @brief 将数据保存到 CSV 文件
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