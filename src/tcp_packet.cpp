#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>

// compute checksum for IP/TCP headers
unsigned short checksum(unsigned short *ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    unsigned short answer;

    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char *)&oddbyte) = *(unsigned char *)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (unsigned short)~sum;

    return answer;
}

// build raw TCP SYN packet
int create_tcp_syn_packet(const char *source_ip, const char *dest_ip,
                          uint16_t source_port, uint16_t dest_port, uint8_t ttl,
                          char *packet_buf, size_t buf_size) {
    if (buf_size < sizeof(iphdr) + sizeof(tcphdr))
        return -1;

    // clear buffer
    memset(packet_buf, 0, buf_size);

    struct iphdr *iph = (struct iphdr *)packet_buf;
    struct tcphdr *tcph = (struct tcphdr *)(packet_buf + sizeof(struct iphdr));

    // IP header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id = htons(54321);
    iph->frag_off = 0;
    iph->ttl = ttl;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;  // filled later
    iph->saddr = inet_addr(source_ip);
    iph->daddr = inet_addr(dest_ip);
    iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

    // tcp header
    tcph->source = htons(source_port);
    tcph->dest = htons(dest_port);
    tcph->seq = htonl(0);
    tcph->ack_seq = 0;
    tcph->doff = 5; // header size
    tcph->fin = 0;
    tcph->syn = 1; // SYN flag
    tcph->rst = 0;
    tcph->psh = 0;
    tcph->ack = 0;
    tcph->urg = 0;
    tcph->window = htons(5840);
    tcph->check = 0; // filled later with checksum
    tcph->urg_ptr = 0;

    // pseudo header for TCP checksum
    struct pseudo_header {
        uint32_t src;
        uint32_t dst;
        uint8_t reserved;
        uint8_t protocol;
        uint16_t length;
    };

    pseudo_header psh;
    psh.src = iph->saddr;
    psh.dst = iph->daddr;
    psh.reserved = 0;
    psh.protocol = IPPROTO_TCP;
    psh.length = htons(sizeof(struct tcphdr));

    // build pseudo packet for checksum
    char pseudo_packet[sizeof(pseudo_header) + sizeof(struct tcphdr)];
    memcpy(pseudo_packet, &psh, sizeof(pseudo_header));
    memcpy(pseudo_packet + sizeof(pseudo_header), tcph, sizeof(struct tcphdr));

    tcph->check = checksum((unsigned short *)pseudo_packet, sizeof(pseudo_packet));

    return sizeof(struct iphdr) + sizeof(struct tcphdr);
}