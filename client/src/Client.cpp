#include "Client.h"
#include <sstream>
#include <cstdlib>

Client::Client(const std::string& serverIp, int port)
    : m_serverIp(serverIp), m_port(port), m_socket(INVALID_SOCKET), m_isConnected(false) {
}

Client::~Client() {
    Disconnect();
}

bool Client::Connect() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup failed\n";
        return false;
    }

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        std::cout << "Error creating socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(m_serverIp.c_str());
    serverAddress.sin_port = htons(m_port);

    if (connect(m_socket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cout << "Connect failed with error: " << WSAGetLastError() << "\n";
        closesocket(m_socket);
        WSACleanup();
        return false;
    }

    m_isConnected = true;
    return true;
}

void Client::Disconnect() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    WSACleanup();
    m_isConnected = false;
}

void Client::Run() {
    std::string line;

    if (!ReceiveMessage(line)) {
        std::cout << "Server disconnected immediately." << std::endl;
        return;
    }

    if (line == "SERVER_BUSY") {
        std::cout << "You have been added to the queue. Please wait..." << std::endl;

        while (true) {
            if (!ReceiveMessage(line)) {
                std::cout << "\nServer disconnected while waiting in queue." << std::endl;
                return;
            }

            if (line == "Welcome to Search Server!") {
                std::cout << "\nA slot has become available. You are now connected." << std::endl;
                break;
            }
        }
    } else {
        std::cout << "Connected to Search Server successfully." << std::endl;
    }

    while (true) {
        std::cout << "\nEnter query (or 'quit' to exit): ";
        std::getline(std::cin, line);

        if (line.empty()) {
            continue;
        }
        if (line == "quit") {
            SendMessage("quit\n");
            break;
        }

        std::string searchCommand = "search " + line;

        std::string header;

        if (!SendMessage(searchCommand) || !ReceiveMessage(header)) {
            std::cout << "Connection error." << std::endl;
            break;
        }

        bool resultsFound = ShowResults(header);

        if (resultsFound) {
            while (true) {
                std::cout << "\nEnter a file index to view snippets, or 'q' to go back: ";
                std::string snippetCommand;
                if (!std::getline(std::cin, snippetCommand)) {
                    break;
                }

                if (snippetCommand == "q" || snippetCommand == "quit") {
                    break;
                }

                if (snippetCommand.find_first_not_of("0123456789") != std::string::npos) {
                    std::cout << "Invalid command. Please enter a number or 'q'." << std::endl;
                    continue;
                }

                if (!SendMessage("getsnippet " + snippetCommand)) {
                    break;
                }

                std::string snippetHeader, snippetData;

                if (ReceiveMessage(snippetHeader) && ReceiveMessage(snippetData)) {
                    ShowSnippets(snippetHeader, snippetData);
                } else {
                    std::cout << "Connection error." << std::endl;
                    break;
                }
            }
        }
    }
}

bool Client::ShowResults(const std::string& header) {
    if (header == "NOT_FOUND") {
        std::cout << "No results found." << std::endl;
        return false;
    }

    if (header.rfind("OK:", 0) == 0) {
        std::string countString = header.substr(3);
        int count = std::atoi(countString.c_str());

        std::cout << "Found " << count << " results:" << std::endl;

        std::string line;
        for (int i = 0; i < count; i++) {
            if (ReceiveMessage(line)) {
                std::cout << "  " << line << std::endl;
            } else {
                std::cout << "Error receiving result line " << i << std::endl;
                break;
            }
        }
        return true;
    }

    std::cout << "Unknown server response: " << header << std::endl;
    return false;
}

void Client::ShowSnippets(const std::string& header, const std::string& data) {
    if (header.find("SNIPPETS_FOUND:") == 0) {
        std::cout << "\n--- Snippets ---" << std::endl;

        std::stringstream ss(data);
        std::string snippet;

        while (std::getline(ss, snippet, ';')) {
            if (!snippet.empty()) {
                std::cout << snippet << std::endl;
                std::cout << "----------------" << std::endl;
            }
        }
    } else {
        std::cout << "Server response: " << header << std::endl;
    }
}

bool Client::ReceiveMessage(std::string& message) {
    message.clear();
    char buffer;

    while (recv(m_socket, &buffer, 1, 0) > 0) {
        if (buffer == '\n') {
            return true;
        }

        if (buffer != '\r') {
            message += buffer;
        }
    }
    return false;
}

bool Client::SendMessage(const std::string& message) {
    if (m_socket == INVALID_SOCKET) {
        return false;
    }

    std::string payload = message;
    if (payload.empty() || payload.back() != '\n') {
        payload += '\n';
    }

    size_t totalSent = 0;
    size_t messageLength = payload.length();

    while (totalSent < messageLength) {
        int sentBytes = send(m_socket, payload.c_str() + totalSent, (int)(messageLength - totalSent), 0);

        if (sentBytes <= 0) {
            return false;
        }

        totalSent += sentBytes;
    }
    return true;
}