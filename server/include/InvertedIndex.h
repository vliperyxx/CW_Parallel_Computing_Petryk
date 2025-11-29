#ifndef INVERTED_INDEX_H
#define INVERTED_INDEX_H

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include "ThreadPool.h"
#include "FileManager.h"

struct WordPosition {
    size_t charOffset{};
    size_t wordOffset{};
};

using write_lock = std::unique_lock<std::shared_mutex>;
using read_lock  = std::shared_lock<std::shared_mutex>;

class InvertedIndex {
public:
    InvertedIndex(ThreadPool* pool, FileManager* fileManager);

    void BuildIndex();
    void RebuildIndex();

    std::vector<std::pair<std::string, std::vector<int>>> Search(const std::string& query) const;

    void Clear();
    size_t Size() const;

private:
    static void Tokenize(const std::string& text, std::vector<std::pair<std::string, WordPosition>>& outTokens);

    std::unordered_map<std::string, std::unordered_map<size_t, std::vector<WordPosition>>> m_index;

    mutable std::shared_mutex m_indexMutex;

    size_t m_indexedCount = 0;

    ThreadPool* m_pool;
    FileManager* m_fileManager;
};

#endif