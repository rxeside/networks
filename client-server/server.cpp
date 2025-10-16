#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <sstream>
#include <cstdio>

#define PORT 5001
#define SERVER_NAME "Server of Ivan Petrov"
#define SERVER_NUMBER 50

/**
 * @brief Инициализирует и настраивает сокет сервера.
 * @return Файловый дескриптор сокета сервера в случае успеха, -1 в случае ошибки.
 */
int setup_server_socket()
{
    int server_fd;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        return -1;
    }
    std::cout << "Socket created.\n";

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        return -1;
    }
    std::cout << "Socket bound to port " << PORT << ".\n";

    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        close(server_fd);
        return -1;
    }
    std::cout << "Server listening for connections...\n";

    return server_fd;
}

/**
 * @brief Обрабатывает подключение клиента.
 * @param new_socket Файловый дескриптор сокета клиента.
 * @return 0 в случае успеха, -1 если получено неверное число от клиента.
 */
int handle_client_connection(int new_socket)
{
    char buffer[1024] = {0};
    ssize_t valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
    if (valread <= 0)
    {
        std::cerr << "Failed to receive data from client.\n";
        return 0;
    }
    buffer[valread] = '\0';
    std::cout << "Received: " << buffer << std::endl;

    std::string msg(buffer);
    size_t pos = msg.find(';');
    if (pos == std::string::npos)
    {
        std::cerr << "Invalid message format.\n";
        return 0;
    }

    std::string client_name = msg.substr(0, pos);
    int client_number = std::stoi(msg.substr(pos + 1));

    if (client_number < 1 || client_number > 100)
    {
        std::cout << "Client sent invalid number (" << client_number << "). Shutting down server.\n";
        return -1;
    }

    int sum = client_number + SERVER_NUMBER;
    std::cout << "Client name: " << client_name << "\n";
    std::cout << "Server name: " << SERVER_NAME << "\n";
    std::cout << "Client number: " << client_number << "\n";
    std::cout << "Server number: " << SERVER_NUMBER << "\n";
    std::cout << "Sum: " << sum << "\n";

    std::ostringstream response;
    response << SERVER_NAME << ";" << SERVER_NUMBER;
    std::string resp_str = response.str();
    send(new_socket, resp_str.c_str(), resp_str.length(), 0);
    std::cout << "Response sent to client.\n";

    return 0;
}

int main()
{
    int server_fd = setup_server_socket();
    if (server_fd < 0)
    {
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0)
        {
            perror("accept failed");
            continue;
        }
        std::cout << "Client connected.\n";

        if (handle_client_connection(new_socket) < 0)
        {
            close(new_socket);
            break;
        }

        close(new_socket);
        std::cout << "Client connection closed.\n";
    }

    close(server_fd);
    return 0;
}