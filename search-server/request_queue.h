//Вставьте сюда своё решение из урока «‎Очередь запросов».‎
#pragma once

#include <deque>

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) :search_server_(search_server), no_result_request_(0), current_time_(0) {
        // напишите реализацию
    }

    std::vector<Document> SetterToAddRequest(const std::vector<Document>& result){
        AddRequest(result.size());
        return result;
    }

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        // напишите реализацию
        const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        return SetterToAddRequest(result);
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const ;
private:
    struct QueryResult {
        // определите, что должно быть в структуре
        int result_;
        uint64_t timestamp;
    };
    
    const SearchServer& search_server_;
    int no_result_request_;
    uint64_t current_time_;
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    // возможно, здесь вам понадобится что-то ещё
    void AddRequest(int results){
    ++current_time_;
    while (!requests_.empty() && sec_in_day_ <= current_time_ - requests_.front().timestamp){
        if(requests_.front().result_ == 0)
        {
            --no_result_request_;
        }
        requests_.pop_front();
    }

    requests_.push_back({results, current_time_});
    if (results == 0) {
        ++no_result_request_;
    }
    }
    };
