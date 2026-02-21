#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>

// Network headers
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>

const char* HELP = R"(
=== LANList - Network Scanner ===

Usage:
    lanlist [options]

Options:
    -h, --help           Show this help message
    -i, --interface IF   Specify network interface (default: first available)
    -t, --timeout SEC    Scan timeout in seconds (default: 2)
    -p, --parallel N     Number of parallel threads (default: 4)
    -n, --no-resolve    Don't resolve hostnames
    -v, --verbose       Show more detailed information

Examples:
    lanlist
    lanlist -i eth0
    lanlist -t 5 -p 8
    lanlist --no-resolve
)";

struct NetworkDevice {
    std::string ip;
    std::string mac;
    std::string hostname;
    bool is_up;
    int response_time;
};

class NetworkScanner {
private:
    std::string interface;
    int timeout;
    int threads;
    bool resolve_names;
    bool verbose;
    std::mutex print_mutex;
    std::vector<NetworkDevice> devices;

    std::string get_default_interface() {
        struct ifaddrs *ifaddr, *ifa;
        std::string result;

        if (getifaddrs(&ifaddr) == -1) {
            throw std::runtime_error("Failed to get network interfaces");
        }

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;

            if (ifa->ifa_addr->sa_family == AF_INET && 
                (ifa->ifa_flags & IFF_UP) && 
                !(ifa->ifa_flags & IFF_LOOPBACK)) {
                result = ifa->ifa_name;
                break;
            }
        }

        freeifaddrs(ifaddr);

        if (result.empty()) {
            throw std::runtime_error("No suitable network interface found");
        }

        return result;
    }

    std::string get_interface_ip() {
        struct ifaddrs *ifaddr, *ifa;
        char host[NI_MAXHOST];

        if (getifaddrs(&ifaddr) == -1) {
            throw std::runtime_error("Failed to get interface IP");
        }

        std::string result;
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;

            if (ifa->ifa_addr->sa_family == AF_INET && 
                strcmp(ifa->ifa_name, interface.c_str()) == 0) {
                getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                           host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                result = host;
                break;
            }
        }

        freeifaddrs(ifaddr);

        if (result.empty()) {
            throw std::runtime_error("Failed to get IP for interface: " + interface);
        }

        return result;
    }

    std::string get_mac_address(const std::string& ip) {
        std::ifstream arp_file("/proc/net/arp");
        if (!arp_file.is_open()) {
            return "unknown";
        }

        std::string line;
        std::getline(arp_file, line); // Skip the header line

        while (std::getline(arp_file, line)) {
            std::istringstream iss(line);
            std::string ip_addr, hw_type, flags, hw_addr;
            
            iss >> ip_addr >> hw_type >> flags >> hw_addr;
            
            if (ip_addr == ip && hw_addr != "00:00:00:00:00:00") {
                return hw_addr;
            }
        }

        return "unknown";
    }

    std::string resolve_hostname(const std::string& ip) {
        if (!resolve_names) return "";

        char hostname[NI_MAXHOST];
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = 0;
        inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

        if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), 
                       hostname, sizeof(hostname), 
                       nullptr, 0, NI_NAMEREQD) == 0) {
            return hostname;
        }

        return "";
    }

    bool ping(const std::string& ip, int& response_time) {
        int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock < 0) {
            if (errno == EPERM) {
                throw std::runtime_error("Root privileges required");
            }
            return false;
        }

        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        struct icmp icmp_hdr;
        icmp_hdr.icmp_type = ICMP_ECHO;
        icmp_hdr.icmp_code = 0;
        icmp_hdr.icmp_cksum = 0;
        icmp_hdr.icmp_seq = 1;
        icmp_hdr.icmp_id = getpid();
        icmp_hdr.icmp_cksum = 0;

        auto start = std::chrono::high_resolution_clock::now();

        if (sendto(sock, &icmp_hdr, sizeof(icmp_hdr), 0,
                  (struct sockaddr*)&addr, sizeof(addr)) <= 0) {
            close(sock);
            return false;
        }

        char buffer[IP_MAXPACKET];
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);

        if (recvfrom(sock, buffer, sizeof(buffer), 0,
                    (struct sockaddr*)&from, &fromlen) <= 0) {
            close(sock);
            return false;
        }

        auto end = std::chrono::high_resolution_clock::now();
        response_time = std::chrono::duration_cast<std::chrono::milliseconds>
                       (end - start).count();

        close(sock);
        return true;
    }

    void scan_range(const std::string& base_ip, int start, int end) {
        std::string ip_prefix = base_ip.substr(0, base_ip.find_last_of('.') + 1);

        for (int i = start; i <= end; i++) {
            std::string ip = ip_prefix + std::to_string(i);
            int response_time = 0;

            if (ping(ip, response_time)) {
                NetworkDevice device;
                device.ip = ip;
                device.mac = get_mac_address(ip);
                device.hostname = resolve_hostname(ip);
                device.is_up = true;
                device.response_time = response_time;

                {
                    std::lock_guard<std::mutex> lock(print_mutex);
                    devices.push_back(device);

                    if (verbose) {
                        std::cout << "Found device: " << ip 
                                 << " (" << device.mac << ")"
                                 << " response time: " << response_time << "ms";
                        if (!device.hostname.empty()) {
                            std::cout << " hostname: " << device.hostname;
                        }
                        std::cout << std::endl;
                    }
                }
            }
        }
    }

public:
    NetworkScanner() : 
        timeout(2),
        threads(4),
        resolve_names(true),
        verbose(false) {
        interface = get_default_interface();
    }

    void set_interface(const std::string& if_name) {
        interface = if_name;
    }

    void set_timeout(int seconds) {
        if (seconds < 1 || seconds > 30) {
            throw std::runtime_error("Timeout must be between 1 and 30 seconds");
        }
        timeout = seconds;
    }

    void set_threads(int n) {
        if (n < 1 || n > 32) {
            throw std::runtime_error("Thread count must be between 1 and 32");
        }
        threads = n;
    }

    void set_resolve_names(bool resolve) {
        resolve_names = resolve;
    }

    void set_verbose(bool v) {
        verbose = v;
    }

    void scan() {
        devices.clear();
        std::string base_ip = get_interface_ip();
        std::cout << "Scanning network on interface " << interface 
                  << " (" << base_ip << ")..." << std::endl;

        std::vector<std::thread> thread_pool;
        int ips_per_thread = 256 / threads;
        
        for (int i = 0; i < threads; i++) {
            int start = i * ips_per_thread;
            int end = (i == threads - 1) ? 255 : start + ips_per_thread - 1;
            thread_pool.emplace_back(&NetworkScanner::scan_range, 
                                   this, base_ip, start, end);
        }

        for (auto& thread : thread_pool) {
            thread.join();
        }

        // Sort devices by IP address
        std::sort(devices.begin(), devices.end(),
                 [](const NetworkDevice& a, const NetworkDevice& b) {
                     return a.ip < b.ip;
                 });

        // Print results
        std::cout << "\nFound " << devices.size() << " active devices:\n\n";

        if (!devices.empty()) {
            // Determine the maximum field widths
            size_t max_ip = 15;  // xxx.xxx.xxx.xxx
            size_t max_mac = 17; // xx:xx:xx:xx:xx:xx
            size_t max_hostname = 0;
            
            for (const auto& device : devices) {
                if (device.hostname.length() > max_hostname) {
                    max_hostname = device.hostname.length();
                }
            }

            // Print table header
            std::cout << std::left 
                     << std::setw(max_ip + 2) << "IP Address"
                     << std::setw(max_mac + 2) << "MAC Address"
                     << std::setw(12) << "Response"
                     << (resolve_names ? "Hostname" : "") << std::endl;
            
            std::cout << std::string(max_ip + max_mac + 14 + 
                      (resolve_names ? max_hostname : 0), '-') << std::endl;

            // Print device rows
            for (const auto& device : devices) {
                std::cout << std::left 
                         << std::setw(max_ip + 2) << device.ip
                         << std::setw(max_mac + 2) << device.mac
                         << std::setw(8) << device.response_time << "ms  "
                         << (resolve_names ? device.hostname : "") << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        NetworkScanner scanner;

        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-h" || args[i] == "--help") {
                std::cout << HELP;
                return 0;
            }
            else if (args[i] == "-i" || args[i] == "--interface") {
                if (++i >= args.size()) {
                    throw std::runtime_error("Interface name required");
                }
                scanner.set_interface(args[i]);
            }
            else if (args[i] == "-t" || args[i] == "--timeout") {
                if (++i >= args.size()) {
                    throw std::runtime_error("Timeout value required");
                }
                scanner.set_timeout(std::stoi(args[i]));
            }
            else if (args[i] == "-p" || args[i] == "--parallel") {
                if (++i >= args.size()) {
                    throw std::runtime_error("Thread count required");
                }
                scanner.set_threads(std::stoi(args[i]));
            }
            else if (args[i] == "-n" || args[i] == "--no-resolve") {
                scanner.set_resolve_names(false);
            }
            else if (args[i] == "-v" || args[i] == "--verbose") {
                scanner.set_verbose(true);
            }
            else {
                throw std::runtime_error("Unknown option: " + args[i]);
            }
        }

        scanner.scan();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Try 'lanlist --help' for more information." << std::endl;
        return 1;
    }
}