//Вставьте сюда своё решение из урока «‎Очередь запросов».‎ 
#include "request_queue.h" 
 
 
 
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) { 
    const auto result = search_server_.FindTopDocuments(raw_query, status); 
        return SetterToAddRequest(result); 
} 
 
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) { 
    const auto result = search_server_.FindTopDocuments(raw_query); 
        return SetterToAddRequest(result); 
} 
 
int RequestQueue::GetNoResultRequests() const { 
    return no_result_request_; 
} 