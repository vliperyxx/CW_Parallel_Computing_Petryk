#include <iostream>
#include <filesystem>
#include "Server.h"
#include "FileManager.h"
#include "InvertedIndex.h"
#include "ThreadPool.h"

int main() {
    const int PORT = 8080;
    const int THREAD_COUNT = 4;
    const int REFRESH_INTERVAL = 60;

    std::string dataDir = DATA_DIR;

    if (!std::filesystem::exists(dataDir) || !std::filesystem::is_directory(dataDir)) {
        std::cout << "Data folder not found at: " << dataDir << std::endl;
        return 1;
    }

    std::vector<std::string> dataFolders = {dataDir};

    FileManager fileManager;
    ThreadPool threadPool;
    InvertedIndex invertedIndex(&threadPool, &fileManager);

    threadPool.Initialize(THREAD_COUNT);
    std::cout << "ThreadPool: Initialized with " << THREAD_COUNT << " threads." << std::endl;

    fileManager.LoadFilePaths(dataFolders);

    std::cout << "Building inverted index..." << std::endl;
    auto indexingStartTime = std::chrono::high_resolution_clock::now();
    invertedIndex.BuildIndex();
    auto indexingEndTime = std::chrono::high_resolution_clock::now();
    auto indexingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(indexingEndTime - indexingStartTime).count();

    std::cout << "InvertedIndex: Index built. Indexed " << invertedIndex.Size() << " words in " << indexingDuration << " ms." << std::endl;

    Server server(PORT, &threadPool, &invertedIndex, &fileManager, dataFolders, REFRESH_INTERVAL);

    if (server.Start()) {
        std::cout << "\nPress Enter to stop the server.\n" << std::endl;

        std::cin.get();
        std::cout << "Stopping server..." << std::endl;
        server.Stop();
    } else {
        std::cout << "Failed to start server." << std::endl;
    }

    threadPool.Terminate();

    return 0;
}