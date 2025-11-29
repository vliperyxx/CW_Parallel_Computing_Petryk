#ifndef SEARCH_RESULT_H
#define SEARCH_RESULT_H

#include <string>
#include <vector>

class SearchResult {
public:
    SearchResult() = default;
    SearchResult(int docId, const std::string& name, const std::string& path, float relevance, const std::vector<int>& positions = {});

    int  GetDocumentId() const;
    const std::string& GetDocumentName() const;
    const std::string& GetPath() const;
    float GetRelevance() const;
    const std::vector<int>& GetSnippetPositions() const;

private:
    int m_documentId = -1;
    std::string m_documentName;
    std::string m_path;
    float m_relevance = 0.0f;
    std::vector<int> m_snippetPositions;
};

#endif