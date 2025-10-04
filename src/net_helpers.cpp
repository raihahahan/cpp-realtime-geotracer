#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iostream>

// https://man7.org/linux/man-pages/man3/gethostbyname.3.html
bool resolve_hostname_ipv4(const char *hostname, std::string &out_ipv4) {
    struct hostent* host_entry;

    // get host information by name
    host_entry = gethostbyname(hostname);

    if (host_entry == nullptr) {
        std::cerr << "Error: Could not resolve hostname " << hostname << std::endl;
        return false;
    }
    
    // convert the first IP address in the list to a human-readable string
    out_ipv4 = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    
    return true;
}

// get local outbound ipv4 address that the kernel would use to reach dest_ip
// dest_ip must be an ipv4 dotted string (eg "8.8.8.8")
// returns true on success and fills out_local_ip with dotted string.
// credits: https://stackoverflow.com/a/49336660
bool get_local_ip_for_dest(const char *dest_ip, std::string &out_local_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (sock < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53); // arbitrary port, using dns port
    if (inet_pton(AF_INET, dest_ip, &remote.sin_addr) != 1) {
        std::cerr << "inet_pton failed for dest_ip\n";
        close(sock);
        return false;
    }

    // connect() does not send packets for UDP. it just sets remote address
    if (connect(sock, (struct sockaddr*)&remote, sizeof(remote)) < 0) {
        perror("connect(udp)");
        close(sock);
        return false;
    }

    struct sockaddr_in local{};
    socklen_t len = sizeof(local);
    if (getsockname(sock, (struct sockaddr*)&local, &len) < 0) {
        perror("getsockname");
        close(sock);
        return false;
    }

    char local_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip))) {
        perror("inet_ntop");
        close(sock);
        return false;
    }

    out_local_ip = local_ip;
    close(sock);
    return true;
}

// obtain an ephemeral (kernel-assigned) TCP source port by binding to port 0
// returns true on success and sets out_port to host-order port number
// using same idea as https://stackoverflow.com/a/49336660
bool get_ephemeral_port(uint16_t &out_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0; // let kernel choose

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return false;
    }

    struct sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (getsockname(sock, (struct sockaddr*)&bound, &len) < 0) {
        perror("getsockname");
        close(sock);
        return false;
    }

    out_port = ntohs(bound.sin_port);
    close(sock);
    return true;
}
