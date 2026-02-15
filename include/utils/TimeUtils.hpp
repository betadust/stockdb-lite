/**
 * @file include/utils/TimeUtils.hpp
 * @author @betadust
 * @date [2026-02-15]
 */

#pragma once

#include <cstdint>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>  // 其实不需要，localtime_s 在 <ctime> 中
#undef min
#undef max // 避免 windows.h 定义的 min/max 宏干扰 std::min/std::max
#endif

namespace high_frequency_storage {
    namespace utils {

        class TimeUtils {
        public:
            static std::string toString(int64_t ms_timestamp) {
                if (ms_timestamp < 0) {
                    return "invalid-timestamp";
                }

                time_t seconds = ms_timestamp / 1000;
                int milliseconds = ms_timestamp % 1000;

                // 使用线程安全的 localtime 版本
                struct tm tm_info;

#ifdef _WIN32
                // Windows 使用 localtime_s
                if (localtime_s(&tm_info, &seconds) != 0) {
                    return "invalid-timestamp";
                }
#else
                // Linux/Mac 使用 localtime_r
                if (!localtime_r(&seconds, &tm_info)) {
                    return "invalid-timestamp";
                }
#endif

                int year = tm_info.tm_year + 1900;
                int month = tm_info.tm_mon + 1;
                int day = tm_info.tm_mday;
                int hour = tm_info.tm_hour;
                int minute = tm_info.tm_min;
                int second = tm_info.tm_sec;

                // 格式化为字符串
                std::string result;

                // 年
                result += std::to_string(year);
                result += "-";

                // 月
                if (month < 10) result += "0";
                result += std::to_string(month);
                result += "-";

                // 日
                if (day < 10) result += "0";
                result += std::to_string(day);
                result += " ";

                // 时
                if (hour < 10) result += "0";
                result += std::to_string(hour);
                result += ":";

                // 分
                if (minute < 10) result += "0";
                result += std::to_string(minute);
                result += ":";

                // 秒
                if (second < 10) result += "0";
                result += std::to_string(second);
                result += ".";

                // 毫秒
                if (milliseconds < 100) {
                    if (milliseconds < 10) {
                        result += "00";
                    }
                    else {
                        result += "0";
                    }
                }
                result += std::to_string(milliseconds);

                return result;
            }

            static int64_t fromString(const std::string& datetime_str) {
                int year, month, day, hour, minute, second;
                int milliseconds = 0;
                char dash1, dash2, colon1, colon2, dot;
				if (datetime_str.length() != 23 && datetime_str.length() != 19) {
                    return -1; // 长度不匹配
                }
                // 尝试完整格式
                std::istringstream iss_full(datetime_str);
                if (iss_full >> year >> dash1 >> month >> dash2 >> day
                    >> hour >> colon1 >> minute >> colon2 >> second
                    >> dot >> milliseconds) {
                    if (dash1 == '-' && dash2 == '-' &&
                        colon1 == ':' && colon2 == ':' && dot == '.') {
                        return toTimestamp(year, month, day, hour, minute, second, milliseconds);
                    }
                }

                // 尝试简写格式
                std::istringstream iss_simple(datetime_str);
                if (iss_simple >> year >> dash1 >> month >> dash2 >> day
                    >> hour >> colon1 >> minute >> colon2 >> second) {
                    if (dash1 == '-' && dash2 == '-' &&
                        colon1 == ':' && colon2 == ':') {
                        return toTimestamp(year, month, day, hour, minute, second, 0);
                    }
                }

                return -1;
            }

            static int64_t now() {
                return static_cast<int64_t>(time(nullptr)) * 1000;
            }

        private:
            static int64_t toTimestamp(int year, int month, int day,
                int hour, int minute, int second,
                int milliseconds) {
                struct tm tm_info = {};
                tm_info.tm_year = year - 1900;
                tm_info.tm_mon = month - 1;
                tm_info.tm_mday = day;
                tm_info.tm_hour = hour;
                tm_info.tm_min = minute;
                tm_info.tm_sec = second;
                tm_info.tm_isdst = -1;


                // 使用线程安全的 mktime（mktime 本身就是线程安全的）
                time_t seconds = mktime(&tm_info);
                if (seconds == -1) {
                    return -1;
                }

                return static_cast<int64_t>(seconds) * 1000 + milliseconds;
            }
        };

    } // namespace utils
} // namespace high_frequency_storage