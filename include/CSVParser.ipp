/**
 * @file include/CSVParser.ipp
 * @brief CSVParser 模板实现
 * @author betadust
 * @date [2026-02-16]
 */

#pragma once

#include "CSVParser.hpp"
#include <cctype>
#include <algorithm>

namespace high_frequency_storage {

template<typename T>
std::vector<T> CSVParser::parseLineAs(const std::string& line, char delimiter) {
    auto fields = parseLine(line, delimiter);
    std::vector<T> result;
    result.reserve(fields.size());
    
    for (const auto& field : fields) {
        std::istringstream iss(field);
        T value;
        if (iss >> value) {
            result.push_back(value);
        } else {
            throw std::invalid_argument("Cannot convert '" + field + "' to target type");
        }
    }
    
    return result;
}

template<typename T>
std::vector<std::vector<T>> CSVParser::parseFileAs(const std::string& filename, 
                                                   char delimiter) {
    std::vector<std::vector<T>> result;
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (isWhitespace(line)) continue;
        
        auto fields = parseLine(line, delimiter);
        std::vector<T> converted;
        converted.reserve(fields.size());
        
        for (const auto& field : fields) {
            std::istringstream iss(field);
            T value;
            if (iss >> value) {
                converted.push_back(value);
            } else {
                throw std::invalid_argument("Cannot convert '" + field + "' at line: " + line);
            }
        }
        
        result.push_back(std::move(converted));
    }
    
    return result;
}

} // namespace high_frequency_storage