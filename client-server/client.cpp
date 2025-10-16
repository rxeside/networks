#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <sstream>

#define PORT 5001
#define CLIENT_NAME "Client of Ivan Petrov"

/**
 * @brief Создает сокет для соединения.
 * @return Файловый дескриптор сокета в случае успеха, -1 в случае ошибки.
 */
int create_socket()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Socket creation error.\n";
    }
    else
    {
        std::cout << "Socket created.\n";
    }
    return sock;
}

/**
 * @brief Устанавливает соединение с сервером.
 * @param sock Файловый дескриптор сокета.
 * @return 0 в случае успеха, -1 в случае ошибки.
 */
int connect_to_server(int sock)
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address / Address not supported.\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Connection failed.\n";
        return -1;
    }
    std::cout << "Connected to server.\n";
    return 0;
}

/**
 * @brief Отправляет сообщение серверу.
 * @param sock Файловый дескриптор сокета.
 * @param client_number Число для отправки.
 */
void send_message(int sock, int client_number)
{
    std::ostringstream msg;
    msg << CLIENT_NAME << ";" << client_number;
    std::string msg_str = msg.str();
    send(sock, msg_str.c_str(), msg_str.length(), 0);
    std::cout << "Message sent to server.\n";
}

/**
 * @brief Получает и обрабатывает ответ от сервера.
 * @param sock Файловый дескриптор сокета.
 * @param client_number Число, отправленное клиентом.
 */
void receive_and_process_response(int sock, int client_number)
{
    char buffer[1024] = {0};
    ssize_t valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (valread <= 0)
    {
        std::cerr << "Failed to receive response from server.\n";
        return;
    }
    buffer[valread] = '\0';
    std::cout << "Received response from server.\n";

    std::string response(buffer);
    size_t pos = response.find(';');
    if (pos == std::string::npos)
    {
        std::cerr << "Invalid server response format.\n";
        return;
    }

    std::string server_name = response.substr(0, pos);
    int server_number = std::stoi(response.substr(pos + 1));

    int sum = client_number + server_number;
    std::cout << "\n--- Results ---\n";
    std::cout << "Client name: " << CLIENT_NAME << "\n";
    std::cout << "Server name: " << server_name << "\n";
    std::cout << "Client number: " << client_number << "\n";
    std::cout << "Server number: " << server_number << "\n";
    std::cout << "Sum: " << sum << "\n";
}

int main()
{
    int client_number;
    std::cout << "Enter an integer between 1 and 100: ";
    std::cin >> client_number;

    if (client_number < 1 || client_number > 100)
    {
        std::cerr << "Number out of range [1, 100].\n";
        return 1;
    }

    int sock = create_socket();
    if (sock < 0)
    {
        return -1;
    }

    if (connect_to_server(sock) < 0)
    {
        close(sock);
        return -1;
    }

    send_message(sock, client_number);
    receive_and_process_response(sock, client_number);

    close(sock);
    std::cout << "Connection closed. Client exiting.\n";

    return 0;
}