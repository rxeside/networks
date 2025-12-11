#include <algorithm>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>

const int BUFFER_SIZE = 8192;
const std::string CACHE_DIR = "cache/";

std::string url_to_filename(const std::string& url) {
    std::string filename = url;
    std::replace(filename.begin(), filename.end(), '/', '_');
    std::replace(filename.begin(), filename.end(), ':', '_');
    std::replace(filename.begin(), filename.end(), '?', '_');
    std::replace(filename.begin(), filename.end(), '&', '_');
    return CACHE_DIR + filename;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    std::string request(buffer);
    std::cout << "--- Received Request ---\n" << request << "\n------------------------\n";

    std::istringstream request_stream(request);
    std::string method, url, http_version;
    request_stream >> method >> url >> http_version;

    if (method != "GET") {
        std::cerr << "Unsupported method: " << method << std::endl;
        close(client_socket);
        return;
    }

    std::string cache_filepath = url_to_filename(url);
    std::ifstream cache_file(cache_filepath, std::ios::binary);

    if (cache_file.is_open()) {
        std::cout << "[INFO] Cache HIT for URL: " << url << std::endl;
        std::vector<char> cache_buffer(BUFFER_SIZE);
        while (!cache_file.eof()) {
            cache_file.read(cache_buffer.data(), cache_buffer.size());
            send(client_socket, cache_buffer.data(), cache_file.gcount(), 0);
        }
        cache_file.close();
    } else {
        std::cout << "[INFO] Cache MISS for URL: " << url << std::endl;

        std::string temp_url = url;
        if (temp_url.rfind("http://", 0) == 0) {
            temp_url = temp_url.substr(7);
        }
        size_t path_pos = temp_url.find('/');
        std::string host = (path_pos == std::string::npos) ? temp_url : temp_url.substr(0, path_pos);
        std::string path = (path_pos == std::string::npos) ? "/" : temp_url.substr(path_pos);

        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            std::cerr << "Could not resolve host: " << host << std::endl;
            close(client_socket);
            return;
        }

        int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in remote_addr;
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = htons(80); // HTTP port
        memcpy(&remote_addr.sin_addr.s_addr, server->h_addr, server->h_length);

        if (connect(remote_socket, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0) {
            perror("Failed to connect to remote server");
            close(remote_socket);
            close(client_socket);
            return;
        }

        std::string forward_request = "GET " + path + " " + http_version + "\r\n"
                                    + "Host: " + host + "\r\n"
                                    + "Connection: close\r\n\r\n"; // "Connection: close" важно!
        send(remote_socket, forward_request.c_str(), forward_request.length(), 0);

        std::ofstream new_cache_file(cache_filepath, std::ios::binary);
        char remote_buffer[BUFFER_SIZE];
        int remote_bytes_received;
        while ((remote_bytes_received = recv(remote_socket, remote_buffer, BUFFER_SIZE, 0)) > 0) {
            send(client_socket, remote_buffer, remote_bytes_received, 0);
            if (new_cache_file.is_open()) {
                new_cache_file.write(remote_buffer, remote_bytes_received);
            }
        }

        if (new_cache_file.is_open()) new_cache_file.close();
        close(remote_socket);
    }

    close(client_socket);
    std::cout << "[INFO] Client connection closed.\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./proxy_server <port>" << std::endl;
        return 1;
    }
    int port = std::stoi(argv[1]);

    if (!std::filesystem::exists(CACHE_DIR)) {
        std::filesystem::create_directory(CACHE_DIR);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        return 1;
    }

    std::cout << "HTTP Proxy server is listening on port " << port << "..." << std::endl;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        std::cout << "[INFO] Accepted new connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}