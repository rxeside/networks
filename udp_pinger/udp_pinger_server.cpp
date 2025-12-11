#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>

const double LOSS_RATE = 0.3;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./udp_pinger_server <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    std::cout << "UDP Pinger Server is listening on port " << port << "..." << std::endl;

    srand(time(nullptr));

    char buffer[1024];
    socklen_t len = sizeof(client_addr);

    while (true) {
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &len);
        if (n < 0) {
            perror("recvfrom error");
            continue;
        }
        buffer[n] = '\0';

        if ((double)rand() / RAND_MAX < LOSS_RATE) {
            std::cout << "Packet from " << inet_ntoa(client_addr.sin_addr) << " lost." << std::endl;
            continue;
        }

        std::cout << "Received: " << buffer << ". Sending echo." << std::endl;
        sendto(sockfd, buffer, n, 0, (const struct sockaddr*)&client_addr, len);
    }

    close(sockfd);
    return 0;
}