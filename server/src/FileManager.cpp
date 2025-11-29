#include "FileManager.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

void FileManager::LoadFilePaths(const std::vector<std::string>& dataFolders) {
    size_t existingCount = m_filePaths.size();

    for (const std::string& folder : dataFolders) {
        if (std::filesystem::exists(folder) && std::filesystem::is_directory(folder)) {
            FindFiles(folder);
        }
    }

    size_t addedCount = m_filePaths.size() - existingCount;
    if (addedCount > 0) {
        std::cout << "FileManager: Found " << addedCount << " new files. Total: " << m_filePaths.size() << std::endl;
    }
}

void FileManager::FindFiles(const std::string& directoryPath) {
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_directory()) {
            FindFiles(entry.path().string());
        } else if (entry.is_regular_file()) {
            std::string path = entry.path().string();

            if (path.size() > 4 && path.substr(path.size() - 4) == ".txt") {
                bool exists = false;
                for (const std::string& existing : m_filePaths) {
                    if (existing == path) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    m_filePaths.push_back(path);
                }
            }
        }
    }
}

std::string FileManager::ReadFileContent(const std::string& path) const {
    std::ifstream file(path);
    if (!file) {
        std::cout << "Could not open file: " + path << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

const std::vector<std::string>& FileManager::GetFilePaths() const {
    return m_filePaths;
}