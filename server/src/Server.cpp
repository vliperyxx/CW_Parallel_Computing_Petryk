#include "Server.h"
#include "ThreadPool.h"
#include "InvertedIndex.h"
#include "SearchResult.h"
#include "Task.h"
#include <sstream>
#include <future>
#include <chrono>

Server::Server(int port, ThreadPool* pool, InvertedIndex* index, FileManager* fm, const std::vector<std::string>& dataFolders, int refreshInterval)
    : m_serverSocket(INVALID_SOCKET), m_port(port), m_isRunning(false), m_pool(pool), m_index(index), m_fileManager(fm), m_dataFolders(dataFolders), m_refreshIntervalSeconds(refreshInterval), m_isStopping(false) {
}

Server::~Server() {
    Stop();
}

bool Server::Start() {
    if (!InitializeServer()) {
        return false;
    }

    m_clientPool = new ThreadPool();
    m_clientPool->Initialize(m_clientThreads);

    if (listen(m_serverSocket, 10000) == SOCKET_ERROR) {
        std::cout << "Listening socket failed: " << WSAGetLastError() << std::endl;
        closesocket(m_serverSocket);
        WSACleanup();
        return false;
    }

    std::cout << "Server started listening on port " << m_port << std::endl;

    m_isRunning = true;
    m_isStopping = false;
    m_acceptThread = std::thread(&Server::AcceptLoop, this);
    m_schedulerThread = std::thread(&Server::SchedulerLoop, this);

    return true;
}

void Server::AcceptLoop() {
    while (m_isRunning) {
        sockaddr_in clientAddress;
        int clientAddressSize = sizeof(clientAddress);

        SOCKET clientSocket = accept(m_serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);

        if (clientSocket == INVALID_SOCKET) {
            if (m_isRunning) {
                std::cout << "Client accept failed: " << WSAGetLastError() << std::endl;
            }
            continue;
        }

        if (m_activeClients.load() < maxActiveClients) {
            m_activeClients++;

            char* clientIp = inet_ntoa(clientAddress.sin_addr);
            int clientPort = ntohs(clientAddress.sin_port);

            std::cout << "Client connected: IP - " << clientIp << ", port - " << clientPort << " (Active: " << m_activeClients.load() << ")\n";

            Task task([this, clientSocket]() {
                this->HandleClient(clientSocket);
            });

            m_clientPool->AddTask(task);
        }
        else {
            Task waitingTask([this, clientSocket]() {
                this->HandleClient(clientSocket);
            });

            m_waitingQueue.Emplace(waitingTask);
            std::cout << "Client added to queue (Queue size: " << m_waitingQueue.Size() << ")\n";

            std::string message = "SERVER_BUSY\n";
            send(clientSocket, message.c_str(), message.length(), 0);
        }
    }
}

void Server::SchedulerLoop() {
    while (!m_isStopping.load()) {
        for (int i = 0; i < m_refreshIntervalSeconds; ++i) {
            if (m_isStopping.load()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (m_isStopping.load()) {
            break;
        }

        std::cout << "\nScheduler: Started updating index." << std::endl;

        m_fileManager->LoadFilePaths(m_dataFolders);
        size_t fileCount = m_fileManager->GetFilePaths().size();
        m_index->BuildIndex();

        std::cout << "Scheduler: Index update complete. " << fileCount << " files indexed." << std::endl;
    }
}

bool Server::InitializeServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup failed" << std::endl;
        return false;
    }

    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(m_port);

    if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cout << "Socket binding failed: " << WSAGetLastError() << std::endl;
        closesocket(m_serverSocket);
        WSACleanup();
        return false;
    }
    return true;
}

void Server::Stop() {
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    m_isStopping = true;

    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    if (m_schedulerThread.joinable()) {
        m_schedulerThread.join();
    }

    if (m_clientPool) {
        m_clientPool->Terminate();
        delete m_clientPool;
        m_clientPool = nullptr;
    }

    WSACleanup();
    std::cout << "Server stopped." << std::endl;
}

void Server::HandleClient(SOCKET clientSocket) {
    std::vector<SearchResult> searchResults;
    std::string lastQuery;
    bool isRunning = true;

    std::string line;
    SendMessage(clientSocket, "Welcome to Search Server!\n");

    while (isRunning && ReceiveMessage(clientSocket, line)) {
        size_t start = line.find_first_not_of(" \r\n\t");
        if (start == std::string::npos) {
            continue;
        }
        size_t end = line.find_last_not_of(" \r\n\t");
        std::string command = line.substr(start, end - start + 1);

        if (command.rfind("search ", 0) == 0) {
            std::string query = command.substr(7);

            auto startTime = std::chrono::high_resolution_clock::now();
            std::vector<std::pair<std::string, std::vector<int>>> rawResults = m_index->Search(query);
            auto endTime = std::chrono::high_resolution_clock::now();
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

            std::cout << "Search: \"" << query << "\" took " << duration << " ms" << std::endl;

            lastQuery = query;
            searchResults.clear();

            for (size_t i = 0; i < rawResults.size(); i++) {
                const std::string& path = rawResults[i].first;
                const std::vector<int>& positions = rawResults[i].second;

                int matches = (int)positions.size();

                std::string fileName;
                std::size_t separatorPos = path.find_last_of("/\\");
                if (separatorPos == std::string::npos) {
                    fileName = path;
                } else {
                    fileName = path.substr(separatorPos + 1);
                }

                SearchResult result((int)i, fileName, path, (float)matches, positions);
                searchResults.push_back(result);
            }

            SendMessage(clientSocket, FormatResults(searchResults));
        }

        else if (command.rfind("getsnippet ", 0) == 0) {
            if (searchResults.empty()) {
                SendMessage(clientSocket, "ERROR_NO_RESULTS\n");
                continue;
            }

            std::string indexString = command.substr(std::string("getsnippet ").size());

            bool isNumber = true;
            for (char symbol : indexString) {
                if (!isdigit((unsigned char)symbol)) {
                    isNumber = false;
                    break;
                }
            }

            if (!isNumber || indexString.empty()) {
                SendMessage(clientSocket, "ERROR_INVALID_INDEX\n");
                continue;
            }

            int fileIndex = std::stoi(indexString);

            if (fileIndex < 0 || (size_t)fileIndex >= searchResults.size()) {
                SendMessage(clientSocket, "ERROR_INVALID_INDEX\n");
                continue;
            }

            const SearchResult& selectedResult = searchResults[fileIndex];
            std::string content = m_fileManager->ReadFileContent(selectedResult.GetPath());

            if (content.empty()) {
                SendMessage(clientSocket, "ERROR_READING_FILE\n");
                continue;
            }
            std::vector<std::string> snippets = BuildSnippets(content, lastQuery, selectedResult.GetSnippetPositions());

            if (snippets.empty()) {
                SendMessage(clientSocket, "ERROR_NO_SNIPPETS\n");
                continue;
            }

            std::ostringstream oss;
            oss << "SNIPPETS_FOUND:" << snippets.size() << "\n";

            for (size_t i = 0; i < snippets.size(); ++i) {
                oss << snippets[i];
                if (i + 1 < snippets.size()) {
                    oss << ";";
                }
            }
            oss << "\n";

            SendMessage(clientSocket, oss.str());
        }

        else if (command == "quit") {
            isRunning = false;
            SendMessage(clientSocket, "BYE\n");
        }
        else {
            SendMessage(clientSocket, "Unknown command\n");
        }
    }

    closesocket(clientSocket);
    m_activeClients--;
    std::cout << "Client disconnected. (Active: " << m_activeClients.load() << ")\n";

    TryProcessNextClient();
}

void Server::TryProcessNextClient() {
    if (!m_clientPool) {
        return;
    }

    if (m_waitingQueue.Size() > 0) {
        Task task = m_waitingQueue.Pop();
        m_activeClients++;
        m_clientPool->AddTask(task);
    }
}

std::vector<std::string> Server::BuildSnippets(const std::string& content, const std::string& query, const std::vector<int>& positions) {
    std::vector<std::string> result;
    if (content.empty() || positions.empty()) {
        return result;
    }

    const int CONTEXT = 40;
    size_t queryLen = query.length();

    std::vector<int> sortedPositions = positions;
    std::sort(sortedPositions.begin(), sortedPositions.end());

    size_t lastEnd = 0;

    for (int position : sortedPositions) {
        if (position < 0) {
            continue;
        }
        size_t startPosition = (size_t)position;
        if (startPosition >= content.size()) {
            continue;
        }

        size_t start = 0;
        if (startPosition > CONTEXT) {
            start = startPosition - CONTEXT;
        }
        size_t end = std::min(content.size(), startPosition + queryLen + CONTEXT);

        if (start < lastEnd && !result.empty()) {
            continue;
        }
        lastEnd = end;

        std::string snippet = content.substr(start, end - start);

        for (char& symbol : snippet) {
            if (symbol == '\n' || symbol == '\r' || symbol == '\t' || symbol == ';') {
                symbol = ' ';
            }
        }

        std::string cleanSnippet;
        bool lastSpace = false;
        for(char symbol : snippet) {
            if (symbol == ' ') {
                if (!lastSpace) {
                    cleanSnippet += ' ';
                    lastSpace = true;
                }
            } else {
                cleanSnippet += symbol;
                lastSpace = false;
            }
        }

        result.push_back(cleanSnippet);
    }
    return result;
}

std::string Server::FormatResults(const std::vector<SearchResult>& results) {
    if (results.empty()) {
        return "NOT_FOUND\n";
    }

    std::stringstream ss;
    ss << "OK:" << results.size() << "\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const SearchResult& r = results[i];
        ss << "[" << i << "] " << r.GetPath() << " | matches=" << r.GetRelevance() << "\n";
    }
    return ss.str();
}

bool Server::ReceiveMessage(SOCKET clientSocket, std::string& message) {
    message.clear();
    char buffer;

    while (recv(clientSocket, &buffer, 1, 0) > 0) {
        if (buffer == '\n') {
            return true;
        }

        if (buffer != '\r') {
            message += buffer;
        }
    }
    return false;
}

bool Server::SendMessage(SOCKET clientSocket, const std::string& message) {
    size_t totalSent = 0;
    size_t messageLength = message.length();

    while (totalSent < messageLength) {
        int sentBytes = send(clientSocket, message.c_str() + totalSent, (int)(messageLength - totalSent), 0);

        if (sentBytes <= 0) {
            return false;
        }

        totalSent += sentBytes;
    }
    return true;
}