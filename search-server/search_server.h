// в качестве заготовки кода используйте последнюю версию своей поисковой системы
#pragma once
#include <cmath>
#include <math.h>
#include <set>
#include <map>
#include <algorithm>
#include <execution>
#include <vector>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
//#include "log_duration.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double PRECISION = 1e-6;
const size_t CONCURENT_MAP_BUCKET_COUNT = 100;

class SearchServer {

public:
    // Defines an invalid document id
    // You can refer this constant as SearchServer::INVALID_DOCUMENT_ID
    template <typename StringContainer>
    explicit SearchServer(const StringContainer&);
    explicit SearchServer(const std::string&);
    explicit SearchServer(std::string_view);

    void AddDocument(int, const std::string_view, DocumentStatus, const std::vector<int>&);

    inline int GetDocumentCount() const noexcept{
        return documents_.size();
    }

    const std::vector<int>::const_iterator begin() const {
        return document_ids_.cbegin();
    }

    const std::vector<int>::const_iterator end() const {
        //auto end() const {
        return document_ids_.cend();
    }

    ////FindTopDocuments
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view, DocumentStatus) const;

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, std::string_view, DocumentStatus) const;

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, std::string_view, DocumentStatus) const;

    std::vector<Document> FindTopDocuments(std::string_view) const;

    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, std::string_view) const;

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, std::string_view) const;

    ////MatchDocument
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view, int) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy policy, const std::string_view raw_query, int document_id) const;


    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy policy, int document_id);

private:

    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string text;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    const TransparentStringSet stop_words_;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;

    std::map<int, std::map<std::string_view, double>> document_id_to_words_freq_ = { {-1, {} } };

    std::map<int, DocumentData> documents_;

    std::vector<int> document_ids_;

    static bool IsValidWord(const std::string_view);

    static int ComputeAverageRating(const std::vector<int>&);

    bool IsStopWord(const std::string_view word) const {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view) const;

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(std::string_view text, bool need_sort) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view) const;

    //FindAllDocuments
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query&, DocumentPredicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query&, DocumentPredicate) const;

};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        :stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
    if(!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)){
        using namespace std;
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);

    sort(std::execution::seq, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < PRECISION){
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query, true);

    auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

    sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < PRECISION){
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(CONCURENT_MAP_BUCKET_COUNT);
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.Erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(CONCURENT_MAP_BUCKET_COUNT); {
        for_each(std::execution::par,
                 query.plus_words.begin(), query.plus_words.end(),
                 [this, &document_to_relevance, &document_predicate](const std::string_view word) {
                     auto it = word_to_document_freqs_.find(word);
                     if (it == word_to_document_freqs_.end()) {
                         return;
                     }
                     const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string{ word });
                     for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                         const auto& document_data = documents_.at(document_id);
                         if (document_predicate(document_id, document_data.status, document_data.rating)) {
                             document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                         }
                     }
                 });
    }

    {
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.Erase(document_id);
        }
    } 
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}
