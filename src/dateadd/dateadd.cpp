#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <vector>
#include <stdexcept>

const char* HELP = R"(
=== DateAdd - Date Calculator ===

Usage:
    dateadd [options] [date] <+/-> <number> <unit>

Options:
    -h, --help          Show this help message
    -f, --format FMT    Output date format (default: %Y-%m-%d)
                        See strftime for format codes
    -u, --utc          Use UTC instead of local time

Units:
    y, year(s)         Years
    m, month(s)        Months
    w, week(s)         Weeks
    d, day(s)          Days
    h, hour(s)         Hours
    min, minute(s)     Minutes
    s, second(s)       Seconds

Date Formats:
    - YYYY-MM-DD
    - YYYY-MM-DD HH:MM:SS
    - now (current date/time)
    - today (current date)
    - yesterday
    - tomorrow

Examples:
    dateadd now + 1 day
    dateadd today + 2 weeks
    dateadd 2024-01-01 + 3 months
    dateadd -f "%Y-%m-%d %H:%M:%S" now + 1 hour
    dateadd -u now + 30 minutes
)";

class DateCalculator {
private:
    std::string format = "%Y-%m-%d";
    bool use_utc = false;
    
    time_t parse_date(const std::string& date_str) {
        if (date_str == "now") {
            return std::time(nullptr);
        }
        
        if (date_str == "today") {
            time_t now = std::time(nullptr);
            struct tm* timeinfo = use_utc ? gmtime(&now) : localtime(&now);
            timeinfo->tm_hour = 0;
            timeinfo->tm_min = 0;
            timeinfo->tm_sec = 0;
            return mktime(timeinfo);
        }
        
        if (date_str == "yesterday") {
            time_t now = std::time(nullptr);
            struct tm* timeinfo = use_utc ? gmtime(&now) : localtime(&now);
            timeinfo->tm_hour = 0;
            timeinfo->tm_min = 0;
            timeinfo->tm_sec = 0;
            timeinfo->tm_mday -= 1;
            return mktime(timeinfo);
        }
        
        if (date_str == "tomorrow") {
            time_t now = std::time(nullptr);
            struct tm* timeinfo = use_utc ? gmtime(&now) : localtime(&now);
            timeinfo->tm_hour = 0;
            timeinfo->tm_min = 0;
            timeinfo->tm_sec = 0;
            timeinfo->tm_mday += 1;
            return mktime(timeinfo);
        }

        struct tm timeinfo = {};
        std::istringstream ss(date_str);
        
        // Try YYYY-MM-DD HH:MM:SS format
        if (date_str.length() > 10) {
            ss >> std::get_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        } else {
            // Try YYYY-MM-DD format
            ss >> std::get_time(&timeinfo, "%Y-%m-%d");
        }
        
        if (ss.fail()) {
            throw std::runtime_error("Invalid date format. Expected YYYY-MM-DD or YYYY-MM-DD HH:MM:SS");
        }
        
        return mktime(&timeinfo);
    }
    
    int parse_number(const std::string& num_str) {
        try {
            return std::stoi(num_str);
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid number: " + num_str);
        }
    }
    
    time_t add_to_date(time_t date, int number, const std::string& unit) {
        struct tm* timeinfo = use_utc ? gmtime(&date) : localtime(&date);
        
        if (unit == "y" || unit == "year" || unit == "years") {
            timeinfo->tm_year += number;
        }
        else if (unit == "m" || unit == "month" || unit == "months") {
            timeinfo->tm_mon += number;
        }
        else if (unit == "w" || unit == "week" || unit == "weeks") {
            timeinfo->tm_mday += number * 7;
        }
        else if (unit == "d" || unit == "day" || unit == "days") {
            timeinfo->tm_mday += number;
        }
        else if (unit == "h" || unit == "hour" || unit == "hours") {
            timeinfo->tm_hour += number;
        }
        else if (unit == "min" || unit == "minute" || unit == "minutes") {
            timeinfo->tm_min += number;
        }
        else if (unit == "s" || unit == "second" || unit == "seconds") {
            timeinfo->tm_sec += number;
        }
        else {
            throw std::runtime_error("Invalid unit: " + unit);
        }
        
        return mktime(timeinfo);
    }

public:
    void set_format(const std::string& fmt) {
        format = fmt;
    }
    
    void set_utc(bool utc) {
        use_utc = utc;
    }
    
    std::string calculate(const std::string& date_str, 
                         const std::string& op,
                         const std::string& num_str,
                         const std::string& unit) {
        time_t date = parse_date(date_str);
        int number = parse_number(num_str);
        
        if (op == "-") {
            number = -number;
        }
        else if (op != "+") {
            throw std::runtime_error("Invalid operator: " + op);
        }
        
        time_t result = add_to_date(date, number, unit);
        struct tm* timeinfo = use_utc ? gmtime(&result) : localtime(&result);
        
        char buffer[100];
        strftime(buffer, sizeof(buffer), format.c_str(), timeinfo);
        return std::string(buffer);
    }
};

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        
        if (args.empty() || args[0] == "-h" || args[0] == "--help") {
            std::cout << HELP;
            return 0;
        }
        
        DateCalculator calc;
        size_t arg_index = 0;
        
        // Parse options
        while (arg_index < args.size() && args[arg_index][0] == '-') {
            if (args[arg_index] == "-f" || args[arg_index] == "--format") {
                if (++arg_index >= args.size()) {
                    throw std::runtime_error("Format string required");
                }
                calc.set_format(args[arg_index++]);
            }
            else if (args[arg_index] == "-u" || args[arg_index] == "--utc") {
                calc.set_utc(true);
                arg_index++;
            }
            else {
                throw std::runtime_error("Unknown option: " + args[arg_index]);
            }
        }
        
        // Validate that all required arguments are present
        if (args.size() - arg_index < 4) {
            throw std::runtime_error("Not enough arguments");
        }
        
        std::string date = args[arg_index++];
        std::string op = args[arg_index++];
        std::string number = args[arg_index++];
        std::string unit = args[arg_index++];
        
        std::string result = calc.calculate(date, op, number, unit);
        std::cout << result << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Try 'dateadd --help' for more information." << std::endl;
        return 1;
    }
}