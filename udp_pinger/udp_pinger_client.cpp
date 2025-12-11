#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <chrono>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <numeric>
#include <algorithm>
#include <iomanip>

long long get_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./udp_pinger_client <server_host> <server_port>" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    int packets_sent = 0;
    int packets_received = 0;
    std::vector<double> rtt_list;

    for (int i = 1; i <= 10; ++i) {
        long long timestamp = get_timestamp_ms();
        std::string message = "Ping " + std::to_string(i) + " " + std::to_string(timestamp);

        auto start_time = std::chrono::high_resolution_clock::now();

        sendto(sockfd, message.c_str(), message.length(), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
        packets_sent++;

        char buffer[1024];
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);

        if (n < 0) {
            std::cout << "Request timed out" << std::endl;
        } else {
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> rtt = end_time - start_time;

            buffer[n] = '\0';
            std::cout << "Ответ от сервера: " << buffer << ", RTT = "
                      << std::fixed << std::setprecision(3) << rtt.count() << " сек" << std::endl;

            packets_received++;
            rtt_list.push_back(rtt.count());
        }
    }

    int packets_lost = packets_sent - packets_received;
    double loss_rate = (double)packets_lost / packets_sent * 100;

    std::cout << "\n--- Статистика ping ---" << std::endl;
    std::cout << "Отправлено: " << packets_sent << ", Получено: " << packets_received
              << ", Потеряно: " << packets_lost << " (" << std::fixed << std::setprecision(1) << loss_rate << "%)" << std::endl;

    if (!rtt_list.empty()) {
        double min_rtt = *std::min_element(rtt_list.begin(), rtt_list.end());
        double max_rtt = *std::max_element(rtt_list.begin(), rtt_list.end());
        double avg_rtt = std::accumulate(rtt_list.begin(), rtt_list.end(), 0.0) / rtt_list.size();

        std::cout << "RTT: мин = " << std::fixed << std::setprecision(3) << min_rtt << "с, "
                  << "макс = " << max_rtt << "с, "
                  << "средн = " << avg_rtt << "с" << std::endl;
    }

    close(sockfd);
    return 0;
}