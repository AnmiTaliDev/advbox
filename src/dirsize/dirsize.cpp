#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace fs = std::filesystem;

const char* HELP = R"(
=== DirSize - Directory Size Calculator ===

Usage:
    dirsize [options] [directory...]

Options:
    -h, --help          Show this help message
    -a, --all          Show hidden files and directories
    -s, --summarize    Display only total for each directory
    -d, --depth N      Max depth of recursion (default: no limit)
    -t, --threshold N  Show only items larger than N (KB)
    -u, --human       Show sizes in human readable format
    --no-sort         Don't sort output by size
    --threads N       Number of threads for calculation (default: 4)

Examples:
    dirsize              # Current directory
    dirsize -u /home    # Home directory with human readable sizes
    dirsize -d 2 /etc   # Etc directory with max depth 2
    dirsize -t 1024     # Show only items larger than 1MB
    dirsize --threads 8  # Use 8 threads for calculation
)";

class DirSize {
private:
    bool show_all;
    bool summarize;
    int max_depth;
    size_t threshold;
    bool human_readable;
    bool sort_output;
    int thread_count;
    std::mutex print_mutex;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool processing_complete = false;
    std::queue<fs::path> work_queue;
    std::map<fs::path, uintmax_t> dir_sizes;

    static std::string format_size(uintmax_t size, bool human) {
        if (!human) {
            return std::to_string(size);
        }

        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double size_d = static_cast<double>(size);

        while (size_d >= 1024.0 && unit < 4) {
            size_d /= 1024.0;
            unit++;
        }

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        if (unit == 0) {
            ss << static_cast<uintmax_t>(size_d);
        } else {
            ss << size_d;
        }
        ss << " " << units[unit];
        return ss.str();
    }

    bool should_process_file(const fs::path& path) {
        if (!show_all && path.filename().string()[0] == '.') {
            return false;
        }
        return true;
    }

    void process_directory(const fs::path& dir, int current_depth) {
        if (max_depth != -1 && current_depth > max_depth) {
            return;
        }

        uintmax_t total_size = 0;
        std::vector<std::pair<fs::path, uintmax_t>> entries;

        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!should_process_file(entry.path())) {
                    continue;
                }

                if (fs::is_directory(entry)) {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    work_queue.push(entry.path());
                    cv.notify_one();
                } else if (fs::is_regular_file(entry)) {
                    try {
                        uintmax_t size = fs::file_size(entry);
                        total_size += size;
                        if (!summarize && size >= threshold) {
                            entries.emplace_back(entry.path(), size);
                        }
                    } catch (const fs::filesystem_error&) {
                        // Skip files that cannot be accessed
                        continue;
                    }
                }
            }
        } catch (const fs::filesystem_error&) {
            // Skip directories that cannot be accessed
            return;
        }

        {
            std::lock_guard<std::mutex> lock(print_mutex);
            dir_sizes[dir] = total_size;

            if (!summarize) {
                if (sort_output) {
                    std::sort(entries.begin(), entries.end(),
                        [](const auto& a, const auto& b) {
                            return a.second > b.second;
                        });
                }

                for (const auto& entry : entries) {
                    if (entry.second >= threshold) {
                        std::cout << std::setw(15) << format_size(entry.second, human_readable)
                                << "  " << entry.first.filename().string() << std::endl;
                    }
                }
            }
        }
    }

    void worker_thread() {
        while (true) {
            fs::path dir_path;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this] { 
                    return !work_queue.empty() || processing_complete; 
                });

                if (processing_complete && work_queue.empty()) {
                    break;
                }

                dir_path = work_queue.front();
                work_queue.pop();
            }

            process_directory(dir_path, 0);
        }
    }

public:
    DirSize() : 
        show_all(false),
        summarize(false),
        max_depth(-1),
        threshold(0),
        human_readable(false),
        sort_output(true),
        thread_count(4) {}

    void set_show_all(bool value) { show_all = value; }
    void set_summarize(bool value) { summarize = value; }
    void set_max_depth(int value) { max_depth = value; }
    void set_threshold(size_t value) { threshold = value * 1024; } // Convert KB to bytes
    void set_human_readable(bool value) { human_readable = value; }
    void set_sort_output(bool value) { sort_output = value; }
    void set_thread_count(int value) { 
        if (value > 0) thread_count = value;
    }

    void process(const std::vector<std::string>& paths) {
        std::vector<std::thread> threads;
        std::vector<fs::path> directories;

        // If no paths were given, use the current directory
        if (paths.empty()) {
            directories.push_back(fs::current_path());
        } else {
            for (const auto& path : paths) {
                if (fs::exists(path)) {
                    directories.push_back(fs::absolute(path));
                } else {
                    std::cerr << "Warning: path does not exist: " << path << std::endl;
                }
            }
        }

        // Start worker threads
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back(&DirSize::worker_thread, this);
        }

        // Enqueue the initial directories
        for (const auto& dir : directories) {
            work_queue.push(dir);
        }

        // Signal that all directories have been submitted
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            processing_complete = true;
        }
        cv.notify_all();

        // Wait for all threads to finish
        for (auto& thread : threads) {
            thread.join();
        }

        // Print results
        if (summarize || !directories.empty()) {
            std::vector<std::pair<fs::path, uintmax_t>> results;
            for (const auto& dir : directories) {
                uintmax_t total = 0;
                for (const auto& [path, size] : dir_sizes) {
                    if (path.string().find(dir.string()) == 0) {
                        total += size;
                    }
                }
                results.emplace_back(dir, total);
            }

            if (sort_output) {
                std::sort(results.begin(), results.end(),
                    [](const auto& a, const auto& b) {
                        return a.second > b.second;
                    });
            }

            std::cout << "\nDirectory sizes:" << std::endl;
            for (const auto& [path, size] : results) {
                if (size >= threshold) {
                    std::cout << std::setw(15) << format_size(size, human_readable)
                            << "  " << path.string() << std::endl;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        DirSize calculator;
        std::vector<std::string> paths;

        size_t i = 0;
        while (i < args.size()) {
            std::string arg = args[i++];
            
            if (arg == "-h" || arg == "--help") {
                std::cout << HELP;
                return 0;
            }
            else if (arg == "-a" || arg == "--all") {
                calculator.set_show_all(true);
            }
            else if (arg == "-s" || arg == "--summarize") {
                calculator.set_summarize(true);
            }
            else if (arg == "-d" || arg == "--depth") {
                if (i >= args.size()) {
                    throw std::runtime_error("--depth requires a number");
                }
                calculator.set_max_depth(std::stoi(args[i++]));
            }
            else if (arg == "-t" || arg == "--threshold") {
                if (i >= args.size()) {
                    throw std::runtime_error("--threshold requires a number");
                }
                calculator.set_threshold(std::stoull(args[i++]));
            }
            else if (arg == "-u" || arg == "--human") {
                calculator.set_human_readable(true);
            }
            else if (arg == "--no-sort") {
                calculator.set_sort_output(false);
            }
            else if (arg == "--threads") {
                if (i >= args.size()) {
                    throw std::runtime_error("--threads requires a number");
                }
                calculator.set_thread_count(std::stoi(args[i++]));
            }
            else if (arg[0] == '-') {
                throw std::runtime_error("Unknown option: " + arg);
            }
            else {
                paths.push_back(arg);
            }
        }

        calculator.process(paths);
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Try 'dirsize --help' for more information." << std::endl;
        return 1;
    }
}