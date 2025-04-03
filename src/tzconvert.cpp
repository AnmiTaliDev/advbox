#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Справка по использованию
const char* HELP = R"(
TZConvert - Time Zone Converter

Usage:
    tzconvert [OPTIONS] <time> [source_timezone] [target_timezone]
    tzconvert -l | --list         List available time zones
    tzconvert -n | --now [tz]     Show current time in timezone (default: local)

Options:
    -h, --help     Show this help message
    -f, --format   Specify output format (default: YYYY-MM-DD HH:MM:SS)
                   %Y - Year, %m - Month, %d - Day
                   %H - Hour, %M - Minute, %S - Second
    -u, --utc      Use UTC as source timezone
    -s, --short    Show only time without date

Time format:
    YYYY-MM-DD HH:MM:SS
    HH:MM:SS (today's date is assumed)
    now (current time)

Examples:
    tzconvert now UTC America/New_York
    tzconvert "2025-02-25 15:30:00" Europe/London Asia/Tokyo
    tzconvert -u -s "14:00:00" America/Los_Angeles
    tzconvert -l
)";

// Структура для времени
struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    
    DateTime() {
        time_t now = time(nullptr);
        struct tm* tm = gmtime(&now);
        year = tm->tm_year + 1900;
        month = tm->tm_mon + 1;
        day = tm->tm_mday;
        hour = tm->tm_hour;
        minute = tm->tm_min;
        second = tm->tm_sec;
    }
};

// Карта смещений популярных часовых поясов (в секундах)
const std::map<std::string, int> TIMEZONE_OFFSETS = {
    {"UTC", 0},
    {"GMT", 0},
    {"America/New_York", -18000},  // UTC-5
    {"America/Los_Angeles", -28800}, // UTC-8
    {"Europe/London", 0},           // UTC+0
    {"Europe/Paris", 3600},         // UTC+1
    {"Europe/Moscow", 10800},       // UTC+3
    {"Asia/Tokyo", 32400},          // UTC+9
    {"Asia/Shanghai", 28800},       // UTC+8
    {"Australia/Sydney", 39600},    // UTC+11
    {"Pacific/Auckland", 43200},    // UTC+12
};

// Функция для парсинга времени из строки
DateTime parse_time(const std::string& time_str) {
    DateTime dt;
    
    if (time_str == "now") {
        return dt;
    }
    
    std::istringstream ss(time_str);
    char delimiter;
    
    if (time_str.length() > 8) { // Полный формат с датой
        ss >> dt.year >> delimiter
           >> dt.month >> delimiter
           >> dt.day >> dt.hour >> delimiter
           >> dt.minute >> delimiter
           >> dt.second;
    } else { // Только время
        ss >> dt.hour >> delimiter
           >> dt.minute >> delimiter
           >> dt.second;
    }
    
    if (ss.fail()) {
        throw std::runtime_error("Invalid time format");
    }
    
    return dt;
}

// Функция для конвертации времени между часовыми поясами
DateTime convert_timezone(const DateTime& dt, 
                        const std::string& from_tz,
                        const std::string& to_tz) {
    // Находим смещения
    auto from_it = TIMEZONE_OFFSETS.find(from_tz);
    auto to_it = TIMEZONE_OFFSETS.find(to_tz);
    
    if (from_it == TIMEZONE_OFFSETS.end() || to_it == TIMEZONE_OFFSETS.end()) {
        throw std::runtime_error("Unknown timezone");
    }
    
    // Вычисляем разницу в секундах
    int offset_diff = to_it->second - from_it->second;
    
    // Конвертируем в struct tm для простоты вычислений
    struct tm tm_time = {};
    tm_time.tm_year = dt.year - 1900;
    tm_time.tm_mon = dt.month - 1;
    tm_time.tm_mday = dt.day;
    tm_time.tm_hour = dt.hour;
    tm_time.tm_min = dt.minute;
    tm_time.tm_sec = dt.second;
    
    // Конвертируем в timestamp, добавляем разницу и конвертируем обратно
    time_t timestamp = mktime(&tm_time);
    timestamp += offset_diff;
    
    struct tm* new_time = gmtime(&timestamp);
    
    DateTime result;
    result.year = new_time->tm_year + 1900;
    result.month = new_time->tm_mon + 1;
    result.day = new_time->tm_mday;
    result.hour = new_time->tm_hour;
    result.minute = new_time->tm_min;
    result.second = new_time->tm_sec;
    
    return result;
}

// Функция форматирования вывода
std::string format_time(const DateTime& dt, const std::string& format, bool short_format) {
    std::ostringstream ss;
    
    if (short_format) {
        ss << std::setfill('0')
           << std::setw(2) << dt.hour << ":"
           << std::setw(2) << dt.minute << ":"
           << std::setw(2) << dt.second;
    } else {
        ss << dt.year << "-"
           << std::setfill('0') << std::setw(2) << dt.month << "-"
           << std::setfill('0') << std::setw(2) << dt.day << " "
           << std::setfill('0') << std::setw(2) << dt.hour << ":"
           << std::setfill('0') << std::setw(2) << dt.minute << ":"
           << std::setfill('0') << std::setw(2) << dt.second;
    }
    
    return ss.str();
}

// Функция вывода списка доступных часовых поясов
void list_timezones() {
    std::cout << "Available time zones:\n\n";
    
    std::map<std::string, std::vector<std::string>> regions;
    for (const auto& tz : TIMEZONE_OFFSETS) {
        std::string region = tz.first.substr(0, tz.first.find('/'));
        if (region == tz.first) {
            region = "Other";
        }
        regions[region].push_back(tz.first);
    }
    
    for (const auto& region : regions) {
        std::cout << region.first << ":\n";
        for (const auto& tz : region.second) {
            int offset = TIMEZONE_OFFSETS.at(tz);
            int hours = offset / 3600;
            int minutes = (std::abs(offset) % 3600) / 60;
            
            std::cout << "  " << std::left << std::setw(20) << tz 
                     << "UTC" << (hours >= 0 ? "+" : "") << hours
                     << (minutes ? ":" + std::to_string(minutes) : "")
                     << "\n";
        }
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    
    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        std::cout << HELP;
        return 0;
    }
    
    if (args[0] == "-l" || args[0] == "--list") {
        list_timezones();
        return 0;
    }
    
    try {
        bool use_utc = false;
        bool short_format = false;
        std::string format = "";
        
        // Разбор опций
        while (!args.empty() && args[0][0] == '-') {
            std::string opt = args[0];
            args.erase(args.begin());
            
            if (opt == "-u" || opt == "--utc") {
                use_utc = true;
            } else if (opt == "-s" || opt == "--short") {
                short_format = true;
            } else if (opt == "-f" || opt == "--format") {
                if (args.empty()) {
                    throw std::runtime_error("Format not specified");
                }
                format = args[0];
                args.erase(args.begin());
            }
        }
        
        if (args.empty()) {
            throw std::runtime_error("Time not specified");
        }
        
        // Парсим время
        DateTime dt = parse_time(args[0]);
        args.erase(args.begin());
        
        // Определяем исходный и целевой часовые пояса
        std::string from_tz = use_utc ? "UTC" : "UTC";
        std::string to_tz = "UTC";
        
        if (!args.empty()) {
            from_tz = args[0];
            args.erase(args.begin());
        }
        
        if (!args.empty()) {
            to_tz = args[0];
        }
        
        // Конвертируем время
        DateTime converted = convert_timezone(dt, from_tz, to_tz);
        
        // Выводим результат
        std::cout << format_time(converted, format, short_format)
                  << " " << to_tz << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Try 'tzconvert --help' for more information.\n";
        return 1;
    }
    
    return 0;
}