#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>

class FileManager {
public:
    FileManager() = default;

    void LoadFilePaths(const std::vector<std::string>& dataFolders);
    std::string ReadFileContent(const std::string& path) const;
    const std::vector<std::string>& GetFilePaths() const;

private:
    std::vector<std::string> m_filePaths;

    void FindFiles(const std::string& directoryPath);
};

#endif