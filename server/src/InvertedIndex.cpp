#include "InvertedIndex.h"
#include <algorithm>
#include <cctype>
#include <atomic>
#include <thread>

InvertedIndex::InvertedIndex(ThreadPool* pool, FileManager* fileManager)
    : m_pool(pool), m_fileManager(fileManager) {
}

void InvertedIndex::BuildIndex() {
    const std::vector<std::string>& filePaths = m_fileManager->GetFilePaths();

    if (filePaths.size() <= m_indexedCount) {
        return;
    }

    std::atomic<int> activeTasks = 0;

    for (size_t i = m_indexedCount; i < filePaths.size(); i++) {
        size_t documentId = i;
        std::string path = filePaths[i];

        activeTasks++;

        Task task([this, documentId, path, &activeTasks]() {
            std::string content = m_fileManager->ReadFileContent(path);

            std::vector<std::pair<std::string, WordPosition>> tokensWithPosition;
            Tokenize(content, tokensWithPosition);

            std::unordered_map<std::string, std::unordered_map<size_t, std::vector<WordPosition>>> localIndex;

            for (const std::pair<std::string, WordPosition>& pair : tokensWithPosition) {
                const std::string& token = pair.first;
                const WordPosition& position = pair.second;
                localIndex[token][documentId].push_back(position);
            }

            {
                write_lock lock(m_indexMutex);
                for (const auto& wordPair : localIndex) {
                    const std::string& word = wordPair.first;
                    const auto& documentMap = wordPair.second;

                    for (const auto& documentPair : documentMap) {
                        size_t docId = documentPair.first;
                        const std::vector<WordPosition>& positions = documentPair.second;

                        m_index[word][docId].insert(m_index[word][docId].end(), positions.begin(), positions.end());
                    }
                }
            }
            activeTasks--;
        });

        m_pool->AddTask(task);
    }
    while (activeTasks > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    m_indexedCount = filePaths.size();
}

void InvertedIndex::RebuildIndex() {
    Clear();
    BuildIndex();
}

void InvertedIndex::Clear() {
    write_lock lock(m_indexMutex);
    m_index.clear();
    m_indexedCount = 0;
}

size_t InvertedIndex::Size() const {
    read_lock lock(m_indexMutex);
    return m_index.size();
}

std::vector<std::pair<std::string, std::vector<int>>> InvertedIndex::Search(const std::string& query) const {
    std::vector<std::pair<std::string, WordPosition>> tokens;
    Tokenize(query, tokens);

    std::vector<std::string> words;
    words.reserve(tokens.size());

    for (const std::pair<std::string, WordPosition>& pair : tokens) {
        words.push_back(pair.first);
    }

    if (words.empty()) {
        return {};
    }

    read_lock lock(m_indexMutex);

    const std::unordered_map<size_t, std::vector<WordPosition>>* rarestWordDocs = nullptr;
    size_t minDocs = SIZE_MAX;
    size_t rarestWordIndex = 0;

    for (size_t i = 0; i < words.size(); i++) {
        const std::string& word = words[i];
        auto wordIter = m_index.find(word);
        if (wordIter == m_index.end()) {
            return {};
        }

        const std::unordered_map<size_t, std::vector<WordPosition>>& docsForWord = wordIter->second;
        if (docsForWord.size() < minDocs) {
            minDocs = docsForWord.size();
            rarestWordDocs = &docsForWord;
            rarestWordIndex = i;
        }
    }

    if (rarestWordDocs == nullptr) {
        return {};
    }

    std::unordered_map<size_t, std::vector<int>> documentMatches;

    for (const auto& documentPair : *rarestWordDocs) {
        size_t documentId = documentPair.first;
        const std::vector<WordPosition>& rarestWordPositions = documentPair.second;

        for (const WordPosition& rarePosition : rarestWordPositions) {
            if (rarePosition.wordOffset < rarestWordIndex) {
                continue;
            }
            size_t baseWordOffset = rarePosition.wordOffset - rarestWordIndex;

            bool sequenceValid = true;
            int phraseStartCharOffset = -1;

            if (rarestWordIndex == 0) {
                phraseStartCharOffset = (int)rarePosition.charOffset;
            }

            for (size_t i = 0; i < words.size(); i++) {
                if (i == rarestWordIndex) {
                    continue;
                }

                const std::string& wordToCheck = words[i];
                const std::unordered_map<size_t, std::vector<WordPosition>>& docsForWord = m_index.at(wordToCheck);

                auto docIter = docsForWord.find(documentId);
                if (docIter == docsForWord.end()) {
                    sequenceValid = false;
                    break;
                }

                const std::vector<WordPosition>& positions = docIter->second;
                size_t targetWordOffset = baseWordOffset + i;
                bool foundAtRightPosition = false;

                int left = 0;
                int right = (int)positions.size() - 1;
                while (left <= right) {
                    int mid = left + (right - left) / 2;
                    if (positions[mid].wordOffset == targetWordOffset) {
                        foundAtRightPosition = true;

                        if (i == 0) {
                            phraseStartCharOffset = (int)positions[mid].charOffset;
                        }
                        break;
                    }
                    if (positions[mid].wordOffset < targetWordOffset) {
                        left = mid + 1;
                    } else {
                        right = mid - 1;
                    }
                }

                if (!foundAtRightPosition) {
                    sequenceValid = false;
                    break;
                }
            }

            if (sequenceValid && phraseStartCharOffset != -1) {
                documentMatches[documentId].push_back(phraseStartCharOffset);
            }
        }
    }

    const std::vector<std::string>& allFilePaths = m_fileManager->GetFilePaths();
    std::vector<std::pair<std::string, std::vector<int>>> results;
    results.reserve(documentMatches.size());

    for (const auto& pair : documentMatches) {
        size_t docId = pair.first;
        const std::vector<int>& positions = pair.second;

        if (docId < allFilePaths.size()) {
            results.emplace_back(allFilePaths[docId], positions);
        }
    }

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
                  if (a.second.size() != b.second.size()) {
                      return a.second.size() > b.second.size();
                  }
                  return a.first < b.first;
    });

    return results;
}

void InvertedIndex::Tokenize(const std::string& text, std::vector<std::pair<std::string, WordPosition>>& outTokens) {
    size_t charOffset = 0;
    size_t wordIndex = 0;
    std::string currentWord;

    for (size_t i = 0; i < text.size(); i++) {
        unsigned char symbol = (unsigned char)text[i];

        if (std::isalnum(symbol)) {
            if (currentWord.empty()) {
                charOffset = i;
            }
            currentWord += ((char)std::tolower(symbol));
        } else {
            if (!currentWord.empty()) {
                WordPosition position;
                position.charOffset = charOffset;
                position.wordOffset = wordIndex;
                outTokens.push_back({currentWord, position});

                currentWord.clear();
                wordIndex++;
            }
        }
    }

    if (!currentWord.empty()) {
        WordPosition position;
        position.charOffset = charOffset;
        position.wordOffset = wordIndex;
        outTokens.push_back({currentWord, position});
    }
}