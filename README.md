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

## 项目结构

stockdb-lite/
├── README.md                    # 项目说明文档
├── data/                        # 示例数据文件夹
│   └── sample_stocks.csv        # 示例股票数据
├── include/                     # 头文件
│   ├── PricePoint.hpp
│   ├── StockSeries.hpp
│   ├── StorageEngine.hpp
│   ├── CSVParser.hpp
│   ├── Compressor.hpp
│   └── utils/
│       └── TimeUtils.hpp
├── src/                         # 源文件
│   ├── PricePoint.cpp
│   ├── StockSeries.cpp
│   ├── StorageEngine.cpp
│   ├── CSVParser.cpp
│   └── Compressor.cpp  
├── tests/                       # 测试文件
│   ├── test_pricepoint.cpp
│   ├── test_stock_series.cpp
│   ├── test_csv_parser.cpp
│   ├── test_storage_engine.cpp
│   ├── test_storage_engine_complete.cpp
│   ├── test_time_utils.cpp
│   ├── test_time_utils._thread.cpp
│   └── test_compression.cpp
└── example/                    # 示例程序
    └── main.cpp                 # 主示例程序

## 架构设计

┌─────────────────────────────────────┐
│          StorageEngine              │  # 存储引擎主类
│  ┌─────────────────────────────┐    │
│  │      StockSeries (AAPL)     │    │  # 每只股票一个系列
│  │  ┌─────────────────────┐    │    │
│  │  │  Active Buffer      │    │    │  # 活跃缓冲区（未压缩）
│  │  └─────────────────────┘    │    │
│  │  ┌─────────────────────┐    │    │
│  │  │ Compressed Segments │    │    │  # 压缩段列表
│  │  │   ┌─────────────┐   │    │    │
│  │  │   │ Segment 1   │   │    │    │  # 每个段独立压缩
│  │  │   └─────────────┘   │    │    │
│  │  │   ┌─────────────┐   │    │    │
│  │  │   │ Segment 2   │   │    │    │
│  │  │   └─────────────┘   │    │    │
│  │  └─────────────────────┘    │    │
│  └─────────────────────────────┘    │
│  ┌─────────────────────────────┐    │
│  │      StockSeries (GOOG)     │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘


## 使用示例

example/main.cpp

