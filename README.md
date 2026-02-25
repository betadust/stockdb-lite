
# StockDB-Lite - 轻量级高频股票数据存储引擎，用现代C++实现

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

StockDB-Lite 是一个专为高频股票数据设计的轻量级列式存储引擎，支持高效的数据压缩、快速查询和简单的 CSV 导入导出功能。

##  特性

- **列式存储**：按股票代码组织数据，支持高效范围查询
- **差分压缩**：采用 ZigZag + Varint 编码，内存占用减少
- **双缓冲区设计**：新数据快速写入，历史数据自动压缩
- **时间序列优化**：严格保证时间戳递增，查询 O(log n)
- **CSV 导入导出**：支持标准 CSV 格式，自动处理表头
- **线程安全**：读写锁设计，支持并发访问
- **实时统计**：O(1) 时间获取最小/最大值、时间范围等统计信息

## 使用示例

example/main.cpp

