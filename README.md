# `traceroute` implementation in C++ with Geolocation

## 1. How it works

1. Entrypoint: create a trace request to a target domain at a specific port
2. `gethostbyname(domain)`: get the domain’s IPv4 address
3. Begin probing with TTL = 1 by sending a TCP SYN packet to the target IP address and port
4. Each router along the path decrements the TTL by 1. When the TTL reaches 0, the router discards the packet and returns an ICMP Time Exceeded message (Type 11)
5. Upon receiving this ICMP response, record the router’s IP address and hop and send another probe with TTL incremented by 1
6. Continue increasing TTL and sending probes until receive a TCP response from the destination (SYN-ACK or RST). This indicates that the final destination has been reached.

## 2. Code overview

```
├── geolocation.cpp
├── main.cpp
├── net_helpers.cpp
├── probe.cpp
├── tcp_packet.cpp
└── utils.cpp
```

1. `main.cpp`: Main entrypoint with TCP send/recv and ICMP send/recv sockets, create probes here
2. `geolocation.cpp`: Get geolocation from IPv4 using http://ip-api.com/json/
3. `net_helpers.cpp`: Helpers to get IPv4 from domain and ephemeral port
4. `probe.cpp`: Main probe logic with helpers to compare ICMP and TCP packetss
5. `tcp_packet.cpp`: Helper to create TCP packet with checksum
6. `utils.cpp`: Utility functions to print RTT summary

## 3. Setup

**Install libcurl**

```
apt-get update
apt-get install libcurl4-openssl-dev
```

**Build src**

```
make
```

**Example run (may need sudo)**

```
sudo ./geotracer google.com

Resolved dst: google.com -> 172.253.63.101
Using local source IP: 172.31.22.32
Using ephemeral source port: 35717
Probing 172.253.63.101 from 172.31.22.32 (src_port=35717, dst_port=443)
Max hops: 30, timeout per probe: 1000 ms

Hop Responder IP         RTT summary (min/avg/max)
----------------------------------------------------------------------
1   240.3.180.11          (Singapore, Local Router)  0.9 ms  1.0 ms  1.1 ms
2   *                     *  *  *
3   72.14.203.158         (Mountain View, California, United States, Google LLC)  1.1 ms  1.1 ms  1.2 ms
4   192.178.44.207        (Mountain View, California, United States, Google LLC)  1.9 ms  1.9 ms  2.0 ms
5   192.178.248.40        (Montreal, Quebec, Canada, Google LLC)  1.3 ms  1.6 ms  1.8 ms
6   142.251.49.192        (Mountain View, California, United States, Google LLC)  1.8 ms  1.8 ms  1.9 ms
7   142.251.226.101       (Mountain View, California, United States, Google LLC)  3.8 ms  4.1 ms  4.2 ms
8   142.250.209.61        (Mountain View, California, United States, Google LLC)  1.9 ms  8.9 ms  21.1 ms
9   172.253.72.25         (Mountain View, California, United States, Google LLC)  1.6 ms  1.7 ms  1.8 ms
10  *                     *  *  *
11  *                     *  *  *
12  *                     *  *  *
13  *                     *  *  *
14  *                     *  *  *
15  *                     *  *  *
16  172.253.63.101        (Mountain View, California, United States, Google LLC)  1.6 ms  1.7 ms  1.7 ms   (DEST)

Done. Destination reached in 16 hops.
```

## 4. Notes

- Only works properly on Linux
- On Mac, may run with Docker but won't work as expected (max only 2 hops). Use Linux VM instead
- But for testing purposes, you may use `dev.sh` script as below:

```
chmod +x dev.sh
./dev.sh build
./dev.sh start
# start the interactive shell
./dev.sh shell
```
