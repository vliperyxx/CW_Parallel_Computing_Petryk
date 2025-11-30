#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include "Queue.h"
#include "SearchResult.h"

#pragma comment(lib, "ws2_32.lib")

class ThreadPool;
class InvertedIndex;
class FileManager;

class Server {
public:
    Server(int port, ThreadPool* pool, InvertedIndex* index, FileManager* fm, const std::vector<std::string>& dataFolders, int refreshInterval);
    ~Server();

    bool Start();
    void Stop();

private:
    bool InitializeServer();
    void AcceptLoop();
    void SchedulerLoop();
    void HandleClient(SOCKET clientSocket);
    void TryProcessNextClient();

    bool ReceiveMessage(SOCKET sock, std::string& out);
    bool SendMessage(SOCKET sock, const std::string& data);
    std::string FormatResults(const std::vector<SearchResult>& results);
    std::vector<std::string> BuildSnippets(const std::string& content, const std::string& query, const std::vector<int>& positions);

    SOCKET m_serverSocket;
    int m_port;
    bool m_isRunning;
    std::thread m_acceptThread;

    ThreadPool* m_pool;
    InvertedIndex* m_index;
    FileManager* m_fileManager;

    std::vector<std::string> m_dataFolders;
    int m_refreshIntervalSeconds;
    std::thread m_schedulerThread;
    std::atomic<int> m_activeSearches{0};
    std::atomic<bool> m_isStopping;

    ThreadPool* m_clientPool = nullptr;
    int m_clientThreads = 4;

    std::atomic<int> m_activeClients{0};
    const int maxActiveClients = 4;
    Queue m_waitingQueue;
};

#endif