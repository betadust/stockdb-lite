/**
 * @file src/StockSeries.cpp
 * @author @betadust
 * @date [2026-09-06]
 *
 * @note 修改说明：
 * - addPrices() 增加了缺失的独占锁
*/

#include "StockSeries.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>
#include <cassert>

namespace high_frequency_storage {

	// @brief 参数构造函数
	// @修改：删除了 time_sorted_ 初始化
	StockSeries::StockSeries(const std::string& code)
		: stock_code_(code) {
		//预分配空间
		active_buffer_.reserve(ACTIVE_BUFFER_LIMIT);
		compressed_segments_.reserve(100); // 预分配段列表空间

		// 初始化统计信息
		min_price_ = std::numeric_limits<double>::max();
		max_price_ = std::numeric_limits<double>::min();
		min_timestamp_ = std::numeric_limits<int64_t>::max();
		max_timestamp_ = std::numeric_limits<int64_t>::min();
		total_points_ = 0;
	}

	// @brief 添加一个价格数据点
	// @修改：增加了严格时间递增检查，删除了 time_sorted_ 相关逻辑
	void StockSeries::addPrice(int64_t timestamp, double price) {
		std::unique_lock lock(rw_mutex_); //独占锁
		// 验证数据有效性
		if (!isValidPricePoint(timestamp, price)) {
			throw std::invalid_argument("Invalid timestamp or price:\n timestamp = "
				+ std::to_string(timestamp) + ", price = " + std::to_string(price)
			);
		}
		// 第一个价格点，设置初始状态min_timestamp
		if (total_points_ == 0) {
			min_timestamp_ = timestamp;
		}
		//判断时间戳必须严格递增
		if (timestamp <= max_timestamp_) {
			throw std::invalid_argument(
				"Timestamp must be strictly increasing"
			);
		}
		// 添加到活跃缓冲区
		active_buffer_.emplace_back(timestamp, price);
		
		//维护状态
		min_price_ = std::min(min_price_, price);
		max_price_ = std::max(max_price_, price);
		max_timestamp_ = timestamp;
		total_points_++;

		// 检查是否需要压缩
		if (active_buffer_.size() >= ACTIVE_BUFFER_LIMIT) {
			compressActiveBuffer();
		}
	}

	// @brief 批量添加价格数据点
	// @修改：增加了严格的批次内和批次间时间递增检查，删除了 time_sorted_ 相关逻辑
	void StockSeries::addPrices(const std::vector<PricePoint>& points) {
		// 独占锁呢？
		std::unique_lock lock(rw_mutex_); //独占锁
		if (points.empty()) return;
		// 1. 先验证所有数据
		for (const auto& point : points) {
			if (!isValidPricePoint(point.getTimestamp(), point.getPrice())) {
				throw std::invalid_argument("Invalid price point in batch");
			}
		}
		// 2. 检查批次内部的时间递增性
		for (size_t i = 1; i < points.size(); ++i) {
			if (points[i].getTimestamp() <= points[i - 1].getTimestamp()) {
				throw std::invalid_argument(
					"Batch timestamps must be strictly increasing. "
					"At index " + std::to_string(i - 1) + " and " + std::to_string(i)
				);
			}
		}
		// 3. 设置初始状态min_timestamp_
		if (total_points_ == 0) {
			min_timestamp_ = points.front().getTimestamp();

		}
		// 4. 检查与现有数据的连接
		if (points.front().getTimestamp() <= max_timestamp_) {
			throw std::invalid_argument(
				"Batch timestamps must be greater than last existing timestamp. "
				"Last: " + std::to_string(max_timestamp_) +
				", First in batch: " + std::to_string(points.front().getTimestamp())
			);
		}
		// 5. 预分配空间
		active_buffer_.reserve(std::min(active_buffer_.size() + points.size(), MAX_SEGMENT_SIZE));
		// 6. 插入并维护状态
		for (auto p : points) {
			active_buffer_.push_back(p);
			min_price_ = std::min(min_price_, p.getPrice());
			max_price_ = std::max(max_price_, p.getPrice());
			//点数超过最大段大小时，先压缩当前活跃缓冲区，再继续添加剩余点
			if (active_buffer_.size() >= MAX_SEGMENT_SIZE) {
				compressActiveBuffer();
			}
		}
		max_timestamp_ = points.back().getTimestamp();
		total_points_ += points.size();
		// 8.检查是否需要压缩
		if (active_buffer_.size() >= ACTIVE_BUFFER_LIMIT) {
			compressActiveBuffer();
		}
	}

	// @brief 查询指定时间范围内的价格列表
	// @修改：压缩段分块查找
	std::vector<double> StockSeries::queryRange(int64_t start_time, int64_t end_time) const {
		std::shared_lock lock(rw_mutex_);
		std::vector<double> result;
		if (start_time > end_time) return result;

		// 1. 查找相关压缩段
		auto [seg_start, seg_end] = findSegmentRange(start_time, end_time);
		
		// 2. 处理压缩段
		if (seg_start != -1) {
			
			std::cout <<  " 压缩段范围 " << seg_start << " " << seg_end << "\n";
			for (size_t i = seg_start; i < seg_end; ++i) {
				auto points = decompressSegment(i);
				for (const auto& p : points) {
					if (p.getTimestamp() >= start_time && p.getTimestamp() <= end_time) {
						result.push_back(p.getPrice());
					}
				}
			}
		}

		// 3. 处理活跃缓冲区
		for (const auto& p : active_buffer_) {
			if (p.getTimestamp() > end_time) break;  // 活跃缓冲区有序
			if (p.getTimestamp() >= start_time) {
				result.push_back(p.getPrice());
			}
		}

		return result;
	}

	// @brief 获取指定时间点的价格（若不存在返回0.0）
	// @修改：不返回最接近数据，只返回完全匹配的价格，删除了 ensureSorted 调用，直接使用二分查找（数据永远有序）
	double StockSeries::getPriceAt(int64_t timestamp) const {
		std::shared_lock lock(rw_mutex_);

		// 1. 先查活跃缓冲区
		if (!active_buffer_.empty()) {
			if (timestamp <= active_buffer_.back().getTimestamp()) {
				// 可能在活跃缓冲区中
				auto it = std::lower_bound(active_buffer_.begin(), active_buffer_.end(), timestamp,
					[](const PricePoint& p, int64_t ts) {
						return p.getTimestamp() < ts;
					});
				if (it != active_buffer_.end() && it->getTimestamp() == timestamp) {
					return it->getPrice();
				}
			}
		}

		// 2. 查压缩段
		int seg_idx = findSegment(timestamp);
		if (seg_idx != -1) {
			auto points = decompressSegment(seg_idx);
			if (timestamp <= points.back().getTimestamp()) {
				auto it = std::lower_bound(points.begin(), points.end(), timestamp,
					[](const PricePoint& p, int64_t ts) {
						return p.getTimestamp() < ts;
					});
				if (it != points.end() && it->getTimestamp() == timestamp) {
					return it->getPrice();
				}
			}
		}
		return 0.0;  // 未找到
	}

	// @brief 获取所有数据（未压缩）
	std::vector<PricePoint> StockSeries::get_all_data() const{
		std::shared_lock lock(rw_mutex_);
		std::vector<PricePoint> result;
		result.reserve(total_points_);
		if (total_points_ == 0) return result;
		// 1. 读取压缩段数据
		for (int i = 0; i < compressed_segments_.size(); i++) {
			auto points = decompressSegment(i);
			result.insert(result.end(), points.begin(), points.end());
		}
		// 2. 读取活跃缓存区
		result.insert(result.end(), active_buffer_.begin(), active_buffer_.end());
		return result;
	}



	// ---------- 数据维护接口 -----------

	// @brief  已删除：不再需要排序
	// void StockSeries::ensureSorted() { ... }  [已删除]


	// @brief 刷新数据，强制压缩活跃缓冲区
	void StockSeries::flush() {
		std::unique_lock lock(rw_mutex_);
		if (!active_buffer_.empty()) {
			compressActiveBuffer();
		}
	}

	// @brief 清空所有数据
	// @修改：删除了 time_sorted_ 相关代码
	void StockSeries::clear() {
		std::unique_lock lock(rw_mutex_);
		active_buffer_.clear();
		compressed_segments_.clear();
		// 重置统计信息
		min_price_ = std::numeric_limits<double>::max();
		max_price_ = std::numeric_limits<double>::min();
		min_timestamp_ = std::numeric_limits<int64_t>::max();
		max_timestamp_ = std::numeric_limits<int64_t>::min();
		total_points_ = 0;
	}

	// @brief 预分配空间
	void StockSeries::reserve(size_t capacity) {
		std::unique_lock lock(rw_mutex_);
		active_buffer_.reserve(capacity);
	}

	// ---------- 序列化接口 ----------

	// @brief 导出为 CSV
	// @修改：无需修改
	std::string StockSeries::toCSV() const {
		std::shared_lock lock(rw_mutex_);
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2);

		// 1. 处理压缩段
		for (size_t i = 0; i < compressed_segments_.size(); ++i) {
			auto points = decompressSegment(i);
			for (const auto& p : points) {
				oss << stock_code_ << ","
					<< p.getTimestamp() << ","
					<< p.getPrice() << "\n";
			}
		}

		// 2. 处理活跃缓冲区
		for (const auto& p : active_buffer_) {
			oss << stock_code_ << ","
				<< p.getTimestamp() << ","
				<< p.getPrice() << "\n";
		}
		return oss.str();
	}

	// @brief 从 CSV 加载
	// @修改：无需修改，因为 addPrice 已经包含时间检查
	size_t StockSeries::fromCSV(const std::string& csv_data) {
		size_t count = 0;
		std::istringstream iss(csv_data);
		std::string line;

		while (std::getline(iss, line)) {
			if (line.empty()) continue;

			size_t pos1 = line.find(',');
			if (pos1 == std::string::npos) continue;

			size_t pos2 = line.find(',', pos1 + 1);
			if (pos2 == std::string::npos) continue;

			std::string code = line.substr(0, pos1);
			if (code != stock_code_) continue;

			try {
				int64_t timestamp = std::stoll(line.substr(pos1 + 1, pos2 - pos1 - 1));
				double price = std::stod(line.substr(pos2 + 1));
				addPrice(timestamp, price);  // addPrice 会检查时间递增
				count++;
			}
			catch (...) {
				continue;
			}
		}
		return count;
	}

	// ---------- 调试接口 ----------

	// @brief 打印各压缩段和活跃缓冲区的前 n 个数据点
	void StockSeries::printHead(size_t n) const {
		std::shared_lock lock(rw_mutex_);

		std::cout << "StockSeries: " << stock_code_ << "\n"
			<< "  Total points: " << size() << "\n"
			<< "  Compressed segments: " << compressed_segments_.size() << "\n"
			<< "  Active buffer: " << active_buffer_.size() << " points\n";

		size_t shown = 0;

		// 显示压缩段的数据
		for (size_t i = 0; i < compressed_segments_.size() && shown < n; ++i) {
			auto points = decompressSegment(i);
			for (const auto& p : points) {
				if (shown++ >= n) break;
				std::cout << "  [" << shown - 1 << "] "
					<< p.getTimestamp() << " -> "
					<< p.getPrice() << "\n";
			}
			shown = 0;
		}

		// 显示活跃缓冲区的数据
		for (const auto& p : active_buffer_) {
			if (shown++ >= n) break;
			std::cout << "  [" << shown - 1 << "] "
				<< p.getTimestamp() << " -> "
				<< p.getPrice() << "\n";
		}
	}

	// @brief 获取内存占用估算值
	size_t StockSeries::memoryUsage() const {
		std::shared_lock lock(rw_mutex_);

		size_t total = sizeof(*this);
		total += active_buffer_.capacity() * sizeof(PricePoint);
		std::cout << "Usage active_buffer: " << total / 1024.0 <<  "KB\n";
		for (const auto& seg : compressed_segments_) {
			total += seg.memoryUsage();
		}
		std::cout << "Usage compressed_segments: " << (total - active_buffer_.capacity() * sizeof(PricePoint)) / 1024.0 << "KB\n";
		return total;
	}

	// ---------- 内部工具函数 ----------

	// @brief 强制压缩活跃缓冲区
	void StockSeries::compressActiveBuffer() {
		if (active_buffer_.empty()) return;

		// 1. 创建压缩段
		CompressedSegment seg;
		Compressor::compressPoints(active_buffer_, seg.block, seg.data);

		// 2. 记录时间范围
		seg.min_timestamp = active_buffer_.front().getTimestamp();
		seg.max_timestamp = active_buffer_.back().getTimestamp();

		// 3. 添加到段列表
		compressed_segments_.push_back(std::move(seg));

		// 4. 清空活跃缓冲区（统计信息已经累加到 stats_，不需要恢复）
		active_buffer_.clear();
	}

	// @brief 解压指定索引的压缩段
	std::vector<PricePoint> StockSeries::decompressSegment(size_t idx) const {
		if (idx >= compressed_segments_.size()) return {};

		const auto& seg = compressed_segments_[idx];
		std::vector<PricePoint> points;
		Compressor::decompressPoints(seg.block, seg.data, points);
		return points;
	}

	// @brief 查找包含指定时间戳的压缩段索引，未找到返回 -1
	int StockSeries::findSegment(int64_t timestamp) const {
		if (compressed_segments_.empty()) return -1;

		// 二分查找
		auto it = std::upper_bound(compressed_segments_.begin(),
			compressed_segments_.end(),
			timestamp,
			[](int64_t ts, const CompressedSegment& seg) {
				return ts < seg.min_timestamp;
			});

		if (it == compressed_segments_.begin()) return -1;

		--it;
		//std::cout << it - compressed_segments_.begin() << " " << timestamp << " " << it->min_timestamp << " " << it->max_timestamp << "\n";

		if (timestamp <= it->max_timestamp) {
			return it - compressed_segments_.begin();
		}
		return -1;
	}

	// @brief 查找与指定时间范围有交集的压缩段索引范围，返回 [start, end]，未找到返回 (-1, -1)
	std::pair<size_t, size_t> StockSeries::findSegmentRange(int64_t start_time, int64_t end_time) const {
		size_t seg_start = compressed_segments_.size() + 1;
		size_t seg_end = 0;

		for (size_t i = 0; i < compressed_segments_.size(); ++i) {
			const auto& seg = compressed_segments_[i];
			// 如果段的时间范围与查询范围有交集，则更新 seg_start 和 seg_end
			if (seg.max_timestamp >= start_time && seg.min_timestamp <= end_time) {
				seg_start = std::min(seg_start, i);
				seg_end = std::max(seg_end, i + 1);
			}
		}
		//std::cout << seg_start << " " << seg_end << " " << start_time << " " << end_time << "\n";
		if (seg_start == compressed_segments_.size() + 1) {
			return { -1, -1 };
		}
		return { seg_start, seg_end };
	}
	// @brief 验证数据点有效性
	bool StockSeries::isValidPricePoint(int64_t timestamp, double price) const {
		return timestamp > 0 && price > 0.0
			&& !std::isnan(price)
			&& !std::isinf(price);
	}

} // namespace high_frequency_storage
