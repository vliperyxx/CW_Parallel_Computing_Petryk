#include "Client.h"
#include <iostream>

int main() {
    const std::string SERVER_IP = "127.0.0.1";
    const int PORT = 8080;

    Client client(SERVER_IP, PORT);

    if (client.Connect()) {
        client.Run();

        std::cout << "Disconnecting..." << std::endl;
        client.Disconnect();
    } else {
        std::cout << "Failed to connect to server." << std::endl;
    }

    return 0;
}