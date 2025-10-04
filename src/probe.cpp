#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h> 
#include <netinet/in.h>  
#include <netinet/ip.h>  
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h> 
#include <unistd.h> 
#include <cstring>      
#include <cerrno>    
#include <cstdint>   
#include <ctime>
#include <sys/time.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

// utils.cpp
double timespec_diff_ms(const struct timespec &a, const struct timespec &b);

// tcp_packet.cpp
int create_tcp_syn_packet(const char *source_ip, const char *dest_ip,
                          uint16_t source_port, uint16_t dest_port, uint8_t ttl,
                          char *packet_buf, size_t buf_size);

// reference: https://sites.uclouvain.be/SystInfo/usr/include/netinet/ip_icmp.h.html
bool match_icmp_with_probe(const char *buf, ssize_t len,
                           const char* probe_src_ip, const char* probe_dst_ip,
                           uint16_t probe_src_port, uint16_t probe_dst_port) {
    // if doesn't contain ip header + icmp header
    if (len < (int)sizeof(struct iphdr) + (int)sizeof(struct icmphdr)) return false;

    // extract ip header
    struct iphdr *outer_iph = (struct iphdr*)buf;
    size_t outer_iph_len = outer_iph->ihl * 4;
    if (len < (ssize_t)(outer_iph_len + sizeof(struct icmphdr))) return false;
    
    // extract icmp header
    struct icmphdr *icmph = (struct icmphdr*)(buf + outer_iph_len);
    // only interested in type 11 (Time Exceeded) or type 3 (Dest Unreachable)
    // printf("ICMP type=%d, code=%d\n", icmph->type, icmph->code);
    if (!(icmph->type == 11 || icmph->type == 3)) return false;

    // the ICMP payload contains the original IP header + first 8 bytes of transport header
    // payload starts at outer_iph_len + sizeof(icmphdr)
    size_t inner_offset = outer_iph_len + sizeof(struct icmphdr);
    if (len < (ssize_t)(inner_offset + sizeof(struct iphdr) + 8)) return false;

    struct iphdr *inner_iph = (struct iphdr*)(buf + inner_offset);
    size_t inner_iph_len = inner_iph->ihl * 4;

    // ensure we have enough bytes for inner tcp header first 8 bytes (source/dest ports)
    if (len < (ssize_t)(inner_offset + inner_iph_len + 8)) return false;

    // get the 8 bytes of TCP header (first two fields contain ports)
    const uint8_t *inner_transport = (const uint8_t*)(buf + inner_offset + inner_iph_len);
    uint16_t inner_src_port = ntohs(*(uint16_t*)(inner_transport + 0));
    uint16_t inner_dst_port = ntohs(*(uint16_t*)(inner_transport + 2));

    // compare IPs and ports: inner IP src/dst correspond to probe IPs for our probe packet
    char inner_src_ip_s[INET_ADDRSTRLEN], inner_dst_ip_s[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &inner_iph->saddr, inner_src_ip_s, sizeof(inner_src_ip_s));
    inet_ntop(AF_INET, &inner_iph->daddr, inner_dst_ip_s, sizeof(inner_dst_ip_s));

    // check ip and ports 
    if (strcmp(inner_src_ip_s, probe_src_ip) == 0 &&
        strcmp(inner_dst_ip_s, probe_dst_ip) == 0 &&
        inner_src_port == probe_src_port &&
        inner_dst_port == probe_dst_port) {
        return true;
    }
    return false;
}

bool match_tcp_with_probe(const char *buf, ssize_t len,
                          const char* probe_src_ip, const char* probe_dst_ip,
                          uint16_t probe_src_port, uint16_t probe_dst_port,
                          bool &is_synack_or_rst) {
    if (len < (int)sizeof(struct iphdr) + (int)sizeof(struct tcphdr)) return false;
    struct iphdr *iph = (struct iphdr*)buf;
    size_t iphdr_len = iph->ihl * 4;
    if (len < (ssize_t)(iphdr_len + sizeof(struct tcphdr))) return false;

    struct tcphdr *tcph = (struct tcphdr*)(buf + iphdr_len);
    char src_s[INET_ADDRSTRLEN], dst_s[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &iph->saddr, src_s, sizeof(src_s));
    inet_ntop(AF_INET, &iph->daddr, dst_s, sizeof(dst_s));

    uint16_t sport = ntohs(tcph->source);
    uint16_t dport = ntohs(tcph->dest);

    // for a reply coming from the destination/hop: source ip == probe_dst_ip, source port == dst_port
    if (strcmp(src_s, probe_dst_ip) == 0 && strcmp(dst_s, probe_src_ip) == 0 &&
        sport == probe_dst_port && dport == probe_src_port) {
        // determine if it's SYN-ACK or RST (destination reached)
        is_synack_or_rst = (tcph->syn && tcph->ack) || tcph->rst;
        return true;
    }
    return false;
}

bool probe_ttl(int send_sock, int recv_icmp_sock, int recv_tcp_sock,
               const char* src_ip, const char* dst_ip, uint16_t src_port, uint16_t dst_port,
               int ttl, int timeout_ms,
               std::string &hop_ip, std::vector<double> &rtts, bool &destination_reached) {
    rtts.clear();
    hop_ip = "-";
    destination_reached = false;
    constexpr int PROBES = 3;

    for (int probe_i = 0; probe_i < PROBES; ++probe_i) {
        char packet[4096];
        int pkt_len = create_tcp_syn_packet(src_ip, dst_ip, src_port, dst_port, (uint8_t)ttl, packet, sizeof(packet));
        if (pkt_len < 0) {
            std::cerr << "create_tcp_syn_packet failed\n";
            return false;
        }

        // send time
        struct timespec t_send;
        clock_gettime(CLOCK_MONOTONIC, &t_send);

        // send packet
        struct sockaddr_in dst_addr{};
        dst_addr.sin_family = AF_INET;
        dst_addr.sin_port = htons(dst_port);
        inet_pton(AF_INET, dst_ip, &dst_addr.sin_addr);

        ssize_t sent = sendto(send_sock, packet, pkt_len, 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));
        if (sent < 0) {
            perror("sendto in probe_ttl");
        }

        // wait for reply(s) with select up to timeout_ms
        int remaining_ms = timeout_ms;
        bool probe_answered = false;
        struct timespec start_time, current_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        
        while (remaining_ms > 0) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(recv_icmp_sock, &rfds);
            FD_SET(recv_tcp_sock, &rfds);
            int maxfd = std::max(recv_icmp_sock, recv_tcp_sock);
            struct timeval tv;
            tv.tv_sec = remaining_ms / 1000;
            tv.tv_usec = (remaining_ms % 1000) * 1000;

            int rv = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
            if (rv < 0) {
                if (errno == EINTR) continue;
                perror("select probe_ttl");
                break;
            } else if (rv == 0) {
                // timeout for this probe
                break;
            }
            
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            int elapsed_ms = (int)timespec_diff_ms(start_time, current_time);
            remaining_ms = timeout_ms - elapsed_ms;

            // got something: check ICMP first
            if (FD_ISSET(recv_icmp_sock, &rfds)) {
                char buf[4096];
                sockaddr_in from{};
                socklen_t fromlen = sizeof(from);
                ssize_t len = recvfrom(recv_icmp_sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
                if (len > 0) {
                    char from_s[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &from.sin_addr, from_s, sizeof(from_s));
                    
                    // verify ICMP corresponds to our probe packet
                    if (match_icmp_with_probe(buf, len, src_ip, dst_ip, src_port, dst_port)) {
                        struct timespec t_recv;
                        clock_gettime(CLOCK_MONOTONIC, &t_recv);
                        double ms = timespec_diff_ms(t_send, t_recv);
                        rtts.push_back(ms);
                        if (hop_ip == "-") hop_ip = std::string(from_s);
                        // continue waiting for potential TCP responses
                        
                        struct iphdr *outer_iph = (struct iphdr*)buf;
                        size_t outer_iph_len = outer_iph->ihl * 4;
                        struct icmphdr *icmph = (struct icmphdr*)(buf + outer_iph_len);
                        // printf("DEBUG: ICMP type=%d, code=%d\n", icmph->type, icmph->code);
                    }
                }
            }

            // check TCP socket for SYN-ACK or RST (destination reached)
            if (!probe_answered && FD_ISSET(recv_tcp_sock, &rfds)) {
                char buf[4096];
                sockaddr_in from{};
                socklen_t fromlen = sizeof(from);
                ssize_t len = recvfrom(recv_tcp_sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
                if (len > 0) {
                    bool is_dest = false;
                    if (match_tcp_with_probe(buf, len, src_ip, dst_ip, src_port, dst_port, is_dest)) {
                        // std::cout << "MATCH TCP\n";
                        struct timespec t_recv;
                        clock_gettime(CLOCK_MONOTONIC, &t_recv);
                        double ms = timespec_diff_ms(t_send, t_recv);
                        rtts.push_back(ms);
                        char from_s[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &from.sin_addr, from_s, sizeof(from_s));
                        if (hop_ip == "-") hop_ip = std::string(from_s);
                        probe_answered = true;
                        if (is_dest) destination_reached = true; // SYN-ACK or RST: destination reached
                    } 
                }
            }
        }

        if (!probe_answered) {
            // record as timeout (-1)
            rtts.push_back(-1.0);
        }
    }

    return destination_reached;
}
    