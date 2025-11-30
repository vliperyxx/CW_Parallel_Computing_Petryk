#ifndef CLIENT_H
#define CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

class Client {
public:
    Client(const std::string& serverIp, int port);
    ~Client();
    bool Connect();
    void Disconnect();
    void Run();

private:
    bool SendMessage(const std::string& message);
    bool ReceiveMessage(std::string& message);

    bool ShowResults(const std::string& header);
    void ShowSnippets(const std::string& header, const std::string& data);

    SOCKET m_socket;
    std::string m_serverIp;
    int m_port;
    bool m_isConnected;
};

#endif