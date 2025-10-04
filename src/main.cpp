#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cerrno>    
#include <ctime>     
#include <sys/time.h>
#include <fcntl.h> 
#include <unistd.h>
#include <numeric>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iomanip>

// net_helpers.cpp
bool resolve_hostname_ipv4(const char *hostname, std::string &out_ipv4);
bool get_local_ip_for_dest(const char *dest_ip, std::string &out_local_ip);
bool get_ephemeral_port(uint16_t &out_port);

// utils.cpp
void print_rtt_summary(const std::vector<double> &rtts, const std::string& location);

// probe.cpp
bool probe_ttl(int send_sock, int recv_icmp_sock, int recv_tcp_sock,
               const char* src_ip, const char* dst_ip, uint16_t src_port, uint16_t dst_port,
               int ttl, int timeout_ms,
               std::string &hop_ip, std::vector<double> &rtts, bool &destination_reached);

// geolocation.cpp
std::string get_geolocation(const std::string& query);

int main(int argc, char** argv) {
    const char* dst_arg;
    uint16_t dst_port;

    if (argc != 2 && argc != 3) {
        std::cout <<  "Usage: ./geotracer <HOSTNAME> <PORT=443>\n";
        return 1;
    }

    if (argc == 2) {
        dst_arg = argv[1];
        dst_port = 443;
    } else if (argc == 3) {
        dst_arg = argv[1];
        dst_port = std::stoi(argv[2]);
    }

    std::string dst_ip;
    if (!resolve_hostname_ipv4(dst_arg, dst_ip)) {
        std::cerr << "Cannot resolve destination\n";
        return 1;
    }
    std::cout << "Resolved dst: " << dst_arg << " -> " << dst_ip << "\n";

    std::string src_ip;
    if (!get_local_ip_for_dest(dst_ip.c_str(), src_ip)) {
        std::cerr << "Cannot determine local outbound IP for " << dst_ip << "\n";
        return 1;
    }
    std::cout << "Using local source IP: " << src_ip << "\n";

    uint16_t src_port;
    if (!get_ephemeral_port(src_port)) {
        std::cerr << "Cannot obtain ephemeral source port\n";
        return 1;
    }
    std::cout << "Using ephemeral source port: " << src_port << "\n";

    int max_hops = (argc >= 6) ? std::stoi(argv[5]) : 30;
    int timeout_ms = (argc >= 7) ? std::stoi(argv[6]) : 1000; // per probe timeout

    int send_tcp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (send_tcp_sock < 0) {
        perror("socket(send_tcp_sock)");
        return 1;
    }

    int one = 1;
    if (setsockopt(send_tcp_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt(IP_HDRINCL)");
        close(send_tcp_sock);
        return 1;
    }

    int recv_icmp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recv_icmp_sock < 0) {
        perror("socket(recv_icmp_sock)");
        close(send_tcp_sock);
        return 1;
    }

    int recv_tcp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (recv_tcp_sock < 0) {
        perror("socket(recv_tcp_sock)");
        close(send_tcp_sock);
        close(recv_icmp_sock);
        return 1;
    }

    std::cout << "Probing " << dst_ip << " from " << src_ip << " (src_port=" << src_port << ", dst_port=" << dst_port << ")\n";
    std::cout << "Max hops: " << max_hops << ", timeout per probe: " << timeout_ms << " ms\n\n";
    std::cout << std::left << std::setw(4) << "Hop" << std::setw(20) << "Responder IP" << " RTT summary (min/avg/max)\n";
    std::cout << std::string(70, '-') << "\n";

    bool overall_destination_reached = false;
    int hop_count = 0;

    for (int ttl = 1; ttl <= max_hops; ++ttl) {
        std::string hop_ip;
        std::vector<double> rtts;
        bool destination_reached = false;
        
        // std::cout << "PROBING WITH TTL: " << ttl << std::endl;
        bool ok = probe_ttl(send_tcp_sock, recv_icmp_sock, recv_tcp_sock,
                            src_ip.c_str(), dst_ip.c_str(), src_port, dst_port,
                            ttl, timeout_ms,
                            hop_ip, rtts, destination_reached);

        if (!ok) {
            // std::cout << "Probe with ttl not successful. Increasing TTL from " << ttl << "\n";
            // std::cerr << "probe_ttl failed for TTL=" << ttl << "\n";
            // break;
        }

        std::string location;
        if (hop_ip != "-" && hop_ip != "*" && !hop_ip.empty()) {
            location = get_geolocation(hop_ip);            
        }

        std::cout << std::setw(4) << ttl;
        if (hop_ip == "-" || hop_ip.empty()) {
            std::cout << std::setw(20) << "*";
        } else {
            std::cout << std::setw(20) << hop_ip;
        }
        print_rtt_summary(rtts, location);
        if (destination_reached) {
            std::cout << "   (DEST)";
        }
        std::cout << "\n";

        hop_count = ttl;
        if (destination_reached) {
            overall_destination_reached = true;
            break;
        }
    }

    std::cout << "\nDone. ";
    if (overall_destination_reached) {
        std::cout << "Destination reached in " << hop_count << " hops.\n";
    } else {
        std::cout << "Destination not reached (max hops " << max_hops << ")\n";
    }

    close(send_tcp_sock);
    close(recv_icmp_sock);
    close(recv_tcp_sock);
    return 0;
}