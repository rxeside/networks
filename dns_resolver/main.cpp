#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <random>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

bool debug_mode = false;

#pragma pack(push, 1)
struct DNS_HEADER {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

struct QUESTION {
    uint16_t qtype;
    uint16_t qclass;
};

struct RES_RECORD {
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t rdlength;
};
#pragma pack(pop)

void domainToDnsFormat(unsigned char* dns, const std::string& hostname) {
    std::string domain = hostname;
    size_t start = 0, end;
    while ((end = domain.find('.', start)) != std::string::npos) {
        std::string label = domain.substr(start, end - start);
        *dns++ = label.length();
        memcpy(dns, label.c_str(), label.length());
        dns += label.length();
        start = end + 1;
    }
    std::string label = domain.substr(start);
    *dns++ = label.length();
    memcpy(dns, label.c_str(), label.length());
    dns += label.length();

    *dns++ = '\0';
}

std::string readDnsName(unsigned char* reader, unsigned char* buffer, int* count) {
    std::string name;
    unsigned int p = 0, jumped = 0;
    *count = 0;

    while (*reader != 0) {
        if (*reader >= 192) {
            unsigned int offset = (*reader & 0x3F) * 256 + *(reader + 1);
            reader = buffer + offset;
            if (!jumped) {
                *count += 2;
            }
            jumped = 1;
        } else {
            unsigned int len = *reader;
            reader++;
            name.append((const char*)reader, len);
            name.append(".");
            reader += len;
            if (!jumped) {
                *count += (len + 1);
            }
        }
    }

    if (!jumped) {
        (*count)++;
    }

    if (!name.empty()) {
        name.pop_back();
    }
    return name;
}


void resolve(const std::string& hostname, int query_type) {
    std::vector<std::string> root_servers = {
        "198.41.0.4",    // a.root-servers.net
        "199.9.14.201",  // b.root-servers.net
        "192.33.4.12",   // c.root-servers.net
    };
    std::string dns_server_ip = root_servers[0];

    if (debug_mode) std::cout << "Starting resolution for " << hostname << " (type " << query_type << ")" << std::endl;

    for (int iteration = 0; iteration < 20; ++iteration) {
        if (debug_mode) std::cout << "\n--- Iteration " << iteration + 1 << " ---" << std::endl;
        if (debug_mode) std::cout << "Querying server: " << dns_server_ip << std::endl;

        int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sockfd < 0) {
            perror("socket creation failed");
            return;
        }

        struct sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(53);
        inet_pton(AF_INET, dns_server_ip.c_str(), &servaddr.sin_addr);

        unsigned char buf[65536];
        DNS_HEADER *dns = (DNS_HEADER*)buf;
        dns->id = (uint16_t)htons(getpid());
        dns->flags = htons(0x0100);
        dns->qdcount = htons(1);
        dns->ancount = 0;
        dns->nscount = 0;
        dns->arcount = 0;

        unsigned char* qname = &buf[sizeof(DNS_HEADER)];
        domainToDnsFormat(qname, hostname);
        size_t qname_len = strlen((const char*)qname) + 1;

        QUESTION *qinfo = (QUESTION*)&buf[sizeof(DNS_HEADER) + qname_len];
        qinfo->qtype = htons(query_type);
        qinfo->qclass = htons(1);

        int query_size = sizeof(DNS_HEADER) + qname_len + sizeof(QUESTION);

        if (sendto(sockfd, buf, query_size, 0, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            perror("sendto failed");
            close(sockfd);
            return;
        }

        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        recvfrom(sockfd, buf, 65536, 0, nullptr, nullptr);
        close(sockfd);

        dns = (DNS_HEADER*)buf;
        unsigned char* reader = &buf[query_size];

        if (debug_mode) {
            std::cout << "Response received. Flags: 0x" << std::hex << ntohs(dns->flags) << std::dec
                      << " Questions: " << ntohs(dns->qdcount)
                      << " Answers: " << ntohs(dns->ancount)
                      << " Authority: " << ntohs(dns->nscount)
                      << " Additional: " << ntohs(dns->arcount) << std::endl;
        }

        if ((ntohs(dns->flags) & 0xF) == 3) {
            std::cout << "Host not found (NXDOMAIN)." << std::endl;
            return;
        }

        if (ntohs(dns->ancount) > 0) {
             for (int i = 0; i < ntohs(dns->ancount); i++) {
                int count;
                std::string name = readDnsName(reader, buf, &count);
                reader += count;

                RES_RECORD* res = (RES_RECORD*)reader;
                reader += sizeof(RES_RECORD);

                if (ntohs(res->type) == query_type) {
                    if (query_type == 1) {
                         struct in_addr addr;
                         memcpy(&addr, reader, sizeof(in_addr));
                         std::cout << hostname << " -> " << inet_ntoa(addr) << std::endl;
                    } else if (query_type == 28) {
                        char ipv6_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, reader, ipv6_str, INET6_ADDRSTRLEN);
                        std::cout << hostname << " -> " << ipv6_str << std::endl;
                    }
                    return;
                }
                reader += ntohs(res->rdlength);
            }
        }

        bool next_server_found = false;
        unsigned char* temp_reader = reader;

        for (int i = 0; i < ntohs(dns->nscount); i++) {
             int count;
             readDnsName(temp_reader, buf, &count);
             temp_reader += count;
             RES_RECORD* res = (RES_RECORD*)temp_reader;
             temp_reader += sizeof(RES_RECORD) + ntohs(res->rdlength);
        }

        for (int i = 0; i < ntohs(dns->arcount); i++) {
            int count;
            std::string name = readDnsName(temp_reader, buf, &count);
            temp_reader += count;

            RES_RECORD* res = (RES_RECORD*)temp_reader;
            temp_reader += sizeof(RES_RECORD);

            if (ntohs(res->type) == 1) {
                struct in_addr addr;
                memcpy(&addr, temp_reader, sizeof(in_addr));
                dns_server_ip = inet_ntoa(addr);
                next_server_found = true;
                if (debug_mode) std::cout << "Found next server to query: " << name << " at " << dns_server_ip << std::endl;
                break;
            }
             temp_reader += ntohs(res->rdlength);
        }

        if (!next_server_found) {
            std::cout << "Could not resolve " << hostname << ". No referral found." << std::endl;
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <hostname> <type (A/AAAA)> [-d]" << std::endl;
        return 1;
    }

    std::string hostname = argv[1];
    std::string type_str = argv[2];
    int query_type;

    if (type_str == "A") {
        query_type = 1;
    } else if (type_str == "AAAA") {
        query_type = 28;
    } else {
        std::cerr << "Unsupported record type: " << type_str << ". Use A or AAAA." << std::endl;
        return 1;
    }

    if (argc > 3 && std::string(argv[3]) == "-d") {
        debug_mode = true;
    }

    resolve(hostname, query_type);

    return 0;
}