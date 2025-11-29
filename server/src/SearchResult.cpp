#include "SearchResult.h"

SearchResult::SearchResult(int docId, const std::string& name, const std::string& path, float relevance, const std::vector<int>& positions)
    : m_documentId(docId), m_documentName(name), m_path(path), m_relevance(relevance), m_snippetPositions(positions) {
}

int  SearchResult::GetDocumentId() const {
    return m_documentId;
}

const std::string& SearchResult::GetDocumentName() const {
    return m_documentName;
}

const std::string& SearchResult::GetPath() const {
    return m_path;
}

float SearchResult::GetRelevance() const {
    return m_relevance;
}

const std::vector<int>& SearchResult::GetSnippetPositions() const {
    return m_snippetPositions;
}