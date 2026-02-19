/** 
 * @file src/StockSeries.cpp
 * @author @betadust
 * @date [2026-02-19]
*/

#pragma once

#include "StockSeries.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>
#include <mutex>

namespace high_frequency_storage {

	// @brief 参数构造函数
	StockSeries::StockSeries(const std::string& code)
		: stock_code_(code), 
		time_sorted_(true), 
		stats_dirty_(true),
		min_price_(0),
		max_price_(0),
		min_timestamp_(0),
		max_timestamp_(0) {
		//预分配少量空间
		metadata_.reserve(1024);
	}

	// @brief 添加一个价格数据点
	void StockSeries::addPrice(int64_t timestamp, double price) {
		if (!isValidPricePoint(timestamp, price)) {
			throw std::invalid_argument("Invalid timestamp or price:\n timestamp = " 
				+ std::to_string(timestamp) + ", price = " + std::to_string(price)
			);
		}
		//检测是否破坏时间排序
		if (time_sorted_ && !metadata_.empty() && timestamp < metadata_.back().getTimestamp()) {
			time_sorted_ = false;
		}
		metadata_.emplace_back(timestamp, price);
		// 标记统计信息为脏
		stats_dirty_ = true; // 统计信息需要更新
	}

	// @brief 批量添加价格数据点
	void StockSeries::addPrices(const std::vector<PricePoint>& points) {
		// 1. 先验证所有数据
		for (const auto& point : points) {
			if (!isValidPricePoint(point.getTimestamp(), point.getPrice())) {
				throw std::invalid_argument("Invalid price point in batch");
			}
		}

		// 2. 再检查有序性影响（但不修改状态）
		bool will_remain_sorted = time_sorted_;
		if (!metadata_.empty() && !points.empty()) {
			if (points.front().getTimestamp() < metadata_.back().getTimestamp()) {
				will_remain_sorted = false;
			}
		}

		// 3. 预分配空间（可能抛出 std::bad_alloc）
		metadata_.reserve(metadata_.size() + points.size());

		// 4. 批量插入（使用 insert 保证强异常安全）
		//    vector::insert 提供强异常保证：如果插入失败，vector 不变
		metadata_.insert(metadata_.end(), points.begin(), points.end());

		// 5. 所有操作成功，最后更新状态
		time_sorted_ = will_remain_sorted;
		stats_dirty_ = true;
	}

	// @brief 查询指定时间范围内的价格
	std::vector<double> StockSeries::queryRange(int64_t start_time, int64_t end_time) const {
		if (start_time > end_time || metadata_.empty()) {
			return {};
		}
		// ensureSorted(); error! const方法不能调用非const方法，
		// 解决方案是将ensureSorted()声明为mutable，或者在queryRange中使用const_cast调用ensureSorted()。
		const_cast<StockSeries*>(this)->ensureSorted(); 
		std::vector<double> result;
		
		//调用私有二分查找工具
		size_t start_idx = lowerBound(start_time);
		size_t end_idx = upperBound(end_time);

		// 提取价格
		result.reserve(end_idx - start_idx);
		for (size_t i = start_idx; i < end_idx; ++i) {
			result.push_back(metadata_[i].getPrice());
		}
		return result;
	}

	// @brief 获取指定时间点的价格（最接近的）
	double StockSeries::getPriceAt(int64_t timestamp) const {
		if (metadata_.empty()) return 0.0;
		size_t idx = lowerBound(timestamp);
		//最左或最右
		if (idx == metadata_.size()) {
			return metadata_.back().getPrice(); // 超出范围，返回最后一个价格
		}
		if (idx == 0) {
			return metadata_[0].getPrice(); // 超出范围，返回第一个价格
		}
		// 返回最接近的价格
		int64_t diff_prev = std::abs(metadata_[idx - 1].getTimestamp() - timestamp);
		int64_t diff_next = std::abs(metadata_[idx].getTimestamp() - timestamp);
		return (diff_prev < diff_next) ? metadata_[idx - 1].getPrice() : metadata_[idx].getPrice();
	}

	// --- 数据维护接口 ---

	// @brief 确保数据按时间戳排序
	void StockSeries::ensureSorted(){
		std::unique_lock lock(mutex_);  // 注意：这里需要写锁，因为可能修改 timme_sorted_
		if (!time_sorted_ && metadata_.size() > 1) {
			std::sort(metadata_.begin(), metadata_.end(),
				[](const PricePoint& a, const PricePoint& b) {
					return a.getTimestamp() < b.getTimestamp();
				});
			time_sorted_ = true;
			stats_dirty_ = true;  // 排序不影响值，但为保险标记脏
		}
	}

	// @brief 清空所有数据
	void StockSeries::clear() {
		metadata_.clear();
		//节省内存
		metadata_.shrink_to_fit();
		time_sorted_ = true;
		stats_dirty_ = true;
	}

	void StockSeries::reserve(size_t capacity) {
		metadata_.reserve(capacity);
	}

	// ---------- 查询接口（统计信息）----------

	double StockSeries::minPrice() const {
		if (metadata_.empty()) {
			return 0.0;
		}
		if (stats_dirty_) {
			refreshStats();
		}
		return min_price_;
	}

	double StockSeries::maxPrice() const {
		if (metadata_.empty()) {
			return 0.0;
		}
		if (stats_dirty_) {
			refreshStats();
		}
		return max_price_;
	}

	int64_t StockSeries::minTimestamp() const {
		if (metadata_.empty()) {
			return 0;
		}
		if (stats_dirty_) {
			refreshStats();
		}
		return min_timestamp_;
	}

	int64_t StockSeries::maxTimestamp() const {
		if (metadata_.empty()) {
			return 0;
		}
		if (stats_dirty_) {
			refreshStats();
		}
		return max_timestamp_;
	}

	// ---------- 序列化接口（可选）----------

	std::string StockSeries::toCSV() const {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2);  // 价格保留两位小数

		for (const auto& point : metadata_) {
			oss << stock_code_ << ","
				<< point.getTimestamp() << ","
				<< point.getPrice() << "\n";
		}

		return oss.str();
	}

	size_t StockSeries::fromCSV(const std::string& csv_data) {
		size_t count = 0;
		std::istringstream iss(csv_data);
		std::string line;

		while (std::getline(iss, line)) {
			// 跳过空行
			if (line.empty()) continue;

			// 简单解析：假设格式为 "code,timestamp,price"
			size_t pos1 = line.find(',');
			if (pos1 == std::string::npos) continue;

			size_t pos2 = line.find(',', pos1 + 1);
			if (pos2 == std::string::npos) continue;

			// 这里我们忽略 code（应该是本对象的 code）
			std::string code = line.substr(0, pos1);
			// 检查是否匹配当前对象的股票代码		
			if (code != stock_code_) continue;

			int64_t timestamp = std::stoll(line.substr(pos1 + 1, pos2 - pos1 - 1));
			double price = std::stod(line.substr(pos2 + 1));

			try {
				addPrice(timestamp, price);
				count++;
			}
			catch (...) {
				// 跳过无效行
				continue;
			}
		}
		return count;
	}

	// ---------- 调试接口 ----------

	void StockSeries::printHead(size_t n) const {
		std::cout << "StockSeries: " << stock_code_
			<< " (total " << metadata_.size() << " points)\n";

		size_t show = std::min(n, metadata_.size());
		for (size_t i = 0; i < show; ++i) {
			std::cout << "  [" << i << "] "
				<< metadata_[i].getTimestamp() << " -> "
				<< metadata_[i].getPrice() << "\n";
		}

		if (show < metadata_.size()) {
			std::cout << "  ...\n";
		}
	}

	size_t StockSeries::memoryUsage() const {
		// vector 容量 * 每个元素大小
		return metadata_.capacity() * sizeof(PricePoint)
			+ stock_code_.capacity()
			+ sizeof(*this);
	}

	// ---------- 内部工具函数 ----------

	// @brief 更新统计信息缓存（调用排序）
	void StockSeries::refreshStats() const {
		if (metadata_.empty()) {
			min_price_ = max_price_ = 0.0;
			min_timestamp_ = max_timestamp_ = 0;
			stats_dirty_ = false;
			return;
		}
		const_cast<StockSeries*>(this)->ensureSorted(); // 确保排序以正确计算时间范围

		min_timestamp_ = metadata_.front().getTimestamp();
		max_timestamp_ = metadata_.back().getTimestamp();

		// 计算价格范围（线性扫描）
		min_price_ = metadata_.front().getPrice();
		max_price_ = metadata_.front().getPrice();


		for (const auto& point : metadata_) {
			double price = point.getPrice();
			if (price < min_price_) min_price_ = price;
			if (price > max_price_) max_price_ = price;
		}
		stats_dirty_ = false; // 已刷新统计信息
	}

	bool StockSeries::isValidPricePoint(int64_t timestamp, double price) const {
		return timestamp > 0 && price > 0.0 
			&& !std::isnan(price)
			&& !std::isinf(price);
	}

	// @brief 二分查找：找到第一个时间戳 >= target 的位置 （需事先有序）
	size_t StockSeries::lowerBound(int64_t target) const {
		auto it = std::lower_bound(metadata_.begin(), metadata_.end(), target,
			[](const PricePoint& pp, int64_t ts) {
				return pp.getTimestamp() < ts;
			});
		return std::distance(metadata_.begin(), it);
	}
	
	// @brief 二分查找：找到第一个时间戳 > target 的位置 （需事先有序）
	size_t StockSeries::upperBound(int64_t target) const {
		auto it = std::upper_bound(metadata_.begin(), metadata_.end(), target,
			[](int64_t ts, const PricePoint& pp) {
				return ts < pp.getTimestamp();
			});
		return std::distance(metadata_.begin(), it);
	}

} // namespace high_frequency_storage

