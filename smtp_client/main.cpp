#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <string>

void sendCommand(int sock, const std::string& cmd) {
    std::cout << "C: " << cmd;
    write(sock, cmd.c_str(), cmd.length());
}

std::string readResponse(int sock) {
    char buffer[2048] = {0};
    read(sock, buffer, sizeof(buffer) - 1);
    std::string response = buffer;
    std::cout << "S: " << response;
    return response;
}

bool checkResponse(const std::string& response, const std::string& expected_code) {
    return response.rfind(expected_code, 0) == 0;
}

int main() {
    const std::string smtp_server = "127.0.0.1";
    const int port = 1025;

    const std::string from_email = "sender@local.com";
    const std::string to_email = "recipient@local.com";
    const std::string client_domain = "myclient.com";

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Ошибка создания сокета");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    struct hostent *host = gethostbyname(smtp_server.c_str());
    if (host == nullptr) {
        perror("Не удалось разрешить имя хоста");
        close(client_socket);
        return 1;
    }
    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка подключения к серверу");
        close(client_socket);
        return 1;
    }

    std::string response = readResponse(client_socket);
    if (!checkResponse(response, "220")) {
        std::cerr << "Сервер не готов или ответил с ошибкой." << std::endl;
        close(client_socket);
        return 1;
    }

    sendCommand(client_socket, "HELO " + client_domain + "\r\n");
    response = readResponse(client_socket);
    if (!checkResponse(response, "250")) {
        std::cerr << "Команда HELO не принята сервером." << std::endl;
        close(client_socket);
        return 1;
    }

    sendCommand(client_socket, "MAIL FROM: <" + from_email + ">\r\n");
    response = readResponse(client_socket);
    if (!checkResponse(response, "250")) {
        std::cerr << "Адрес отправителя не принят." << std::endl;
        close(client_socket);
        return 1;
    }

    sendCommand(client_socket, "RCPT TO: <" + to_email + ">\r\n");
    response = readResponse(client_socket);
    if (!checkResponse(response, "250")) {
        std::cerr << "Адрес получателя не принят." << std::endl;
        close(client_socket);
        return 1;
    }

    sendCommand(client_socket, "DATA\r\n");
    response = readResponse(client_socket);
    if (!checkResponse(response, "354")) {
        std::cerr << "Сервер не готов принять тело письма." << std::endl;
        close(client_socket);
        return 1;
    }

    std::string email_body =
        "From: " + from_email + "\r\n"
        "To: " + to_email + "\r\n"
        "Subject: Test Email from C++ SMTP Client\r\n"
        "\r\n"
        "Hello!\r\n"
        "This is a test message sent from my simple C++ SMTP client.\r\n"
        "Best regards.\r\n";

    email_body += ".\r\n";
    sendCommand(client_socket, email_body);
    response = readResponse(client_socket);
    if (!checkResponse(response, "250")) {
        std::cerr << "Письмо не было принято сервером." << std::endl;
        close(client_socket);
        return 1;
    }

    sendCommand(client_socket, "QUIT\r\n");
    response = readResponse(client_socket);
    if (!checkResponse(response, "221")) {
        std::cerr << "Сессия не была корректно завершена." << std::endl;
    } else {
        std::cout << "\nПисьмо успешно отправлено!" << std::endl;
    }

    close(client_socket);

    return 0;
}