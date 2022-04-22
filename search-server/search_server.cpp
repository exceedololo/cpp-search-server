// в качестве заготовки кода используйте последнюю версию своей поисковой системы
#include "search_server.h"
#include <future>
#include <numeric>

SearchServer::SearchServer(std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}

SearchServer::SearchServer(const std::string& stop_words_text)
        : SearchServer(std::string_view(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status,
                               const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id");
    }
    //std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const auto [it, inserted] = documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, std::string(document) });
    const auto words = SplitIntoWordsNoStop(it->second.text);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_id_to_words_freq_[document_id][word] += inv_word_count;
    }
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {

    return FindTopDocuments(
            std::execution::seq,
            raw_query,
            [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query, DocumentStatus status) const {

    return FindTopDocuments(
            std::execution::seq,
            raw_query,
            [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query, DocumentStatus status) const {

    return FindTopDocuments(
            std::execution::par,
            raw_query,
            [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query) const{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query) const{
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query, true);

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return{{}, documents_.at(document_id).status};
        }
    }

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    //return { matched_words, documents_.at(document_id).status };
    return { std::vector<std::string_view>(matched_words.begin(), matched_words.end()), documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy policy, const std::string_view raw_query, int document_id) const{
    return SearchServer::MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy, const std::string_view raw_query, int document_id) const{
    const auto query = ParseQuery(raw_query, false);

    const auto word_checker =
            [this, document_id](const std::string_view word){
                const auto it = word_to_document_freqs_.find(word);
                return it != word_to_document_freqs_.end() && it->second.count(document_id);
            };
    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), word_checker)){
        return{{}, documents_.at(document_id).status};
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(
            std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            matched_words.begin(),
            word_checker);
    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());


    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsValidWord(const std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const{
    using namespace std::string_literals;

    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s + word.data() + " is invalid"s);
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty");
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word // is invalid"); // /*" + text + "*/
    }
    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text, bool need_sort) const
{
    Query query;
    auto  vector_words = SplitIntoWords(text);

    for (const std::string_view word : vector_words) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    if (need_sort)
    {
        /*std::sort(std::execution::par, vector_words.begin(), vector_words.end());
        auto last = std::unique(vector_words.begin(), vector_words.end());
        vector_words.resize(std::distance(vector_words.begin(), last));*/
        for (auto* words : {&query.plus_words, &query.minus_words}){
            sort(words->begin(), words->end());
            words->erase(unique(words->begin(),words->end()), words->end());
        }
    }
    return query;
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    auto it = document_id_to_words_freq_.find(document_id);

    std::map<std::string_view, double>* result = new std::map<std::string_view, double>;

    if (it != document_id_to_words_freq_.end()) {
        result->insert(it->second.begin(), it->second.end());
        /*
        for (auto& [word, freq] : it->second) {
            result.emplace(std::string_view(word), freq);
        }
        */
    }
    return *result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_id_to_words_freq_.count(document_id)){

        for (auto& [_, documentIdToTF] : word_to_document_freqs_) {
            if (documentIdToTF.count(document_id)) {
                documentIdToTF.erase(document_id);
            }
        }
        document_id_to_words_freq_.erase(document_id);
        documents_.erase(document_id);
        auto iter = find(document_ids_.begin(), document_ids_.end(), document_id);
        document_ids_.erase(iter);
    }}

void SearchServer::RemoveDocument(std::execution::sequenced_policy policy, int document_id) {
    RemoveDocument( document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy policy, int document_id) {
    /* if (document_id_to_words_freq_.count(document_id)){

         const auto& word_freqs = document_id_to_words_freq_.at(document_id);
         std::vector<const std::string*> words(word_freqs.size());

         std::transform(policy, word_freqs.begin(), word_freqs.end(), words.begin(),
                  [](const auto& item){
                      return &item.first;
                  });

         std::for_each(policy, words.begin(), words.end(), [&document_id, this](const auto& word){
             word_to_document_freqs_.at(*word).erase(document_id);
         });
         document_id_to_words_freq_.erase(document_id);
         documents_.erase(document_id);}}*/
    /*if (document_id_to_words_freq_.count(document_id)) {

        this->document_id_to_words_freq_.erase(document_id);
        this->documents_.erase(document_id);

        auto it = std::lower_bound(document_ids_.begin(), document_ids_.end(), document_id);
        std::future<void> f3 = std::async([it, this] {this->document_ids_.erase(it); });

        std::for_each(policy, word_to_document_freqs_.begin(), word_to_document_freqs_.end(), [document_id](auto& word) {
            if (word.second.find(document_id) != word.second.end()) {
                word.second.erase(document_id);
            }
            });
    }*/

    if (documents_.count(document_id) == 0)
    {
        return;
    }
    //document_ids_.erase(document_id);

    documents_.erase(document_id);

    auto word_freq = std::move(document_id_to_words_freq_.at(document_id));
    std::vector<std::string_view> words(word_freq.size());
    std::transform(std::execution::par,
                   word_freq.begin(), word_freq.end(), words.begin(),
                   [](const auto w_f) {
                       return w_f.first;
                   });

    std::for_each(std::execution::par, words.begin(), words.end(),
                  [&document_id, this](const auto word) {
                      word_to_document_freqs_.at(word).erase(document_id);
                  });

    document_id_to_words_freq_.erase(document_id);}
