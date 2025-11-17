#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

void handle_client(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);

    std::cout << "--- Received Request ---" << std::endl;
    std::cout << buffer << std::endl;
    std::cout << "------------------------" << std::endl;


    std::string request(buffer);
    std::istringstream request_stream(request);
    std::string method, path, http_version;
    request_stream >> method >> path >> http_version;

    if (method == "GET") {
        if (path.substr(0, 1) == "/") {
            path = path.substr(1);
        }
        if (path.empty()) {
            path = "index.html";
        }

        std::ifstream file(path);
        if (file.good()) {
            std::stringstream file_buffer;
            file_buffer << file.rdbuf();
            std::string content = file_buffer.str();
            file.close();

            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/html\r\n";
            response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
            response += "\r\n";
            response += content;

            write(client_socket, response.c_str(), response.length());
            std::cout << "Responded with 200 OK for file: " << path << std::endl;
        } else {
            std::string content = "File Not Found";
            std::string response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Type: text/plain\r\n";
            response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
            response += "\r\n";
            response += content;

            write(client_socket, response.c_str(), response.length());
            std::cout << "Responded with 404 Not Found for file: " << path << std::endl;
        }
    }

    close(client_socket);
    std::cout << "Client connection closed." << std::endl << std::endl;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return 1;
    }

    int port = 8080;
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        return 1;
    }

    std::cout << "Server is listening on port " << port << "..." << std::endl;

    while (true) {
        sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        std::cout << "New connection accepted." << std::endl;

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}