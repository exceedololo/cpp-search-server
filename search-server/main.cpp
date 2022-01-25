#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int Global_Const = 1e-6;

const int ACCURACY = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

   template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);
        
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < ACCURACY) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status;});
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if /*(documents_.at(document_id).status == status)*/ (predicate(document_id, documents_.at(document_id).status,documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};

/* 
   Подставьте сюда вашу реализацию макросов 
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TEST>
void RunTestImpl(TEST Test1, const string& func) {
    Test1();
    cerr << func;
    cerr <<" OK" << endl;
}

#define RUN_TEST(func) RunTestImpl ((func), #func)
 
// -------- Начало модульных тестов поисковой системы ----------
 
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
 
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}
 
void TestAddDocument(){
    SearchServer server;
    {
      const string query = "in the city"s;
	  const auto found_docs = server.FindTopDocuments(query);
	  ASSERT(found_docs.empty());
    }
	{
      const string query = "in the city"s;
      server.AddDocument(1, "in the city"s, DocumentStatus::ACTUAL, {});
      const auto found_docs = server.FindTopDocuments(query);
	  ASSERT(found_docs.size() == 1U);
      ASSERT(found_docs.front().id == 1); // хотябы id сверить
    }
}
 
void TestStopWordsExclude() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	SearchServer server;
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	const auto found_docs = server.FindTopDocuments("in"s);
	ASSERT(found_docs.size() == 1);
	const Document& doc0 = found_docs[0];
	ASSERT(doc0.id == doc_id); //проверяем что документ по запросу находится верный
	server.SetStopWords("in the"s);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	ASSERT(server.FindTopDocuments("in"s).empty());//проверяем что документ по запросу не находится, т.к. содержит стоп слова
}
 
void TestMinusWords() {
    SearchServer server_test;
 
    server_test.SetStopWords("и"s);
 
    server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
 
    ASSERT(server_test.GetDocumentCount() == 1);
 
    ASSERT(server_test.FindTopDocuments("кот -модный"s).empty());
    ASSERT(server_test.FindTopDocuments("-модный"s).empty());
    ASSERT(server_test.FindTopDocuments(""s).empty());
    ASSERT(server_test.FindTopDocuments("мышь -хомяк"s).empty());
    ASSERT(server_test.FindTopDocuments("мышь"s).empty());
    ASSERT(server_test.FindTopDocuments("-мышь"s).empty());
    ASSERT(server_test.FindTopDocuments("кот -кот"s).empty());
    ASSERT(server_test.FindTopDocuments("мышь -мышь"s).empty());
 
    ASSERT(server_test.FindTopDocuments("кот"s).size() == 1U);
    ASSERT(server_test.FindTopDocuments("кот"s)[0].id == 0);
    auto find_only_plus_word = server_test.MatchDocument("кот"s, 0);
    tuple<vector<string>, DocumentStatus> document_for_find_only_plus_word_test = { {"кот"s}, DocumentStatus::ACTUAL };
    ASSERT(find_only_plus_word == document_for_find_only_plus_word_test);
}
 
void TestMatchingWords() {
 
    SearchServer server_test;
 
    server_test.SetStopWords("и"s);
 
    server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
 
    ASSERT(server_test.GetDocumentCount() == 1);
 
    auto find_only_plus_word = server_test.MatchDocument("белый и ошейник"s, 0);
    tuple<vector<string>, DocumentStatus> document_for_find_only_plus_word_test = { {"белый"s, "ошейник"s}, DocumentStatus::ACTUAL };
    ASSERT(find_only_plus_word == document_for_find_only_plus_word_test);
 
    auto find_with_minus_word = server_test.MatchDocument("-белый и ошейник"s, 0);
    tuple<vector<string>, DocumentStatus> document_for_find_with_minus_word_test = { {}, DocumentStatus::ACTUAL };
    ASSERT(find_with_minus_word == document_for_find_with_minus_word_test);
}
 
void TestRelevanceSort() {
    SearchServer server_test;
 
    server_test.SetStopWords("и"s);
 
    server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server_test.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server_test.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server_test.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    server_test.AddDocument(4, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 18, -3 });
    server_test.AddDocument(5, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 12, 7 });
    server_test.AddDocument(6, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, -2, 1 });
    server_test.AddDocument(7, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { -9 });
    server_test.AddDocument(8, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -13 });
    server_test.AddDocument(9, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 27 });
    server_test.AddDocument(10, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 31 });
    server_test.AddDocument(11, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9, 5 });
 
    ASSERT(server_test.GetDocumentCount() == 12U);
 
    auto find = server_test.FindTopDocuments("пушистый ухоженный кот"s);
 
    ASSERT(find.size() == 5U);
 
    vector<double> document_rel;
    for (int i = 0; i < find.size(); ++i) {
        document_rel.push_back(find[i].relevance);
    }
 
    ASSERT(is_sorted(begin(document_rel), end(document_rel), [](double l, double r) {return l > r; }) == 1);
}
 
void TestRating()
{
    SearchServer server;
 
    server.AddDocument(0, "a b c", DocumentStatus::ACTUAL, { 1, 2, 3});
 
    ASSERT( server.FindTopDocuments( "a"s )[0].rating == 2 );
 
    server.AddDocument(1, "d e f", DocumentStatus::ACTUAL, { 1, 2});
 
    ASSERT( server.FindTopDocuments( "d"s )[0].rating == 1 );
 
    server.AddDocument(2, "g h i", DocumentStatus::ACTUAL, { 1, -3});
 
    ASSERT( server.FindTopDocuments( "i"s )[0].rating == -1 );
}
 
//return FindTopDocuments(raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]]	int rating) { return document_status == status; });
 
void TestPredicate() {
    SearchServer server;
 
    server.AddDocument(0, "a b c", DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "c d e", DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "e f g", DocumentStatus::ACTUAL, {2});
 
    ASSERT( server.FindTopDocuments( "a"s, []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating ){ return status == DocumentStatus::ACTUAL; })[0].id == 0);
    ASSERT( server.FindTopDocuments( "a"s, []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating ){ return status == DocumentStatus::BANNED; }).empty() );
 
    ASSERT( server.FindTopDocuments( "c"s, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating ){ return rating < 2; }).size() == 2 );
    ASSERT( server.FindTopDocuments( "c"s, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating ){ return rating == 2; }).empty() );
    ASSERT( server.FindTopDocuments( "c"s, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating ){ return rating == 1; })[0].id == 1 );
}
 
void TestStatus() {
    SearchServer server_test;
 
    server_test.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server_test.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::BANNED, { 8, -3 });
    server_test.AddDocument(2, "белый кот и модный ошейник"s, DocumentStatus::IRRELEVANT, { 8, -3 });
    server_test.AddDocument(3, "белый кот и модный ошейник"s, DocumentStatus::REMOVED, { 8, -3 });
 
    ASSERT(server_test.GetDocumentCount() == 4);
 
    string query = "кот модный"s;
 
    auto find = server_test.FindTopDocuments(query);
    ASSERT(find.size() == 1U);
    ASSERT(find[0].id == 0);
 
    auto find_actual = server_test.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT(find_actual.size() == 1U);
    ASSERT(find_actual[0].id == 0);
 
    auto find_banned = server_test.FindTopDocuments(query, DocumentStatus::BANNED);
    ASSERT(find_banned.size() == 1U);
    ASSERT(find_banned[0].id == 1);
 
    auto find_irrelevant = server_test.FindTopDocuments("кот модный"s, DocumentStatus::IRRELEVANT);
    ASSERT(find_irrelevant.size() == 1U);
    ASSERT(find_irrelevant[0].id == 2);
 
    auto find_removed = server_test.FindTopDocuments("кот модный"s, DocumentStatus::REMOVED);
    ASSERT(find_removed.size() == 1U);
    ASSERT(find_removed[0].id == 3);
}
 
struct TestingDocs {
    const int id;
    const string text;
    DocumentStatus status;
    const vector<int> ratings;
};

void TestRelevanceSearchedDocumentContent() {
    vector<TestingDocs> documents_for_test = {
        /* 1 */ { 37, "пушистый кот и пушистый хвост"s,     DocumentStatus::ACTUAL, {3, 3, 4, 3, 5}},    // ср.рейтинг 3,6
        /* 2 */ { 42, "белый но кот модный ошейник"s,       DocumentStatus::ACTUAL, { 1, 2, 3 } },       // ср.рейтинг 2
        /* 3 */ { 55, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 1, 4, 3, 4 } },    // ср.рейтинг 3
        /* 4 */ { 11, "ухоженный но скворец евгений"s,      DocumentStatus::ACTUAL, {4, 4, 5, 5} },      // ср.рейтинг 4,5
        /* 5 */ { 23, "модный пушистый скворец"s,           DocumentStatus::ACTUAL, {3, 2, 4, 1} },      // ср.рейтинг 2,5
        /* 6 */ { 45, "белый кот и модный ошейник"s,        DocumentStatus::BANNED, { 1, 1, 1, 1, 1 } }, // ср.рейтинг 1
    };
    SearchServer server;
    server.SetStopWords("и но"s);
    for (const auto [doc_id, content, status, ratings] : documents_for_test)
        server.AddDocument(doc_id, content, status, ratings);
 
    const auto found_docs = server.FindTopDocuments("и модный кот");
    vector<double> test_relevance = { 0.34657, 0.23104, 0.17328 };
    for (int i = 0; i < 3; i++)
        ASSERT(fabs(test_relevance[i] - found_docs[i].relevance) < ACCURACY);
}
 
/*
Разместите код остальных тестов здесь
*/
 
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestStopWordsExclude);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchingWords);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestRating);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevanceSearchedDocumentContent);
    // Не забудьте вызывать остальные тесты здесь
}
 
// --------- Окончание модульных тестов поисковой системы -----------


// ==================== для примера =========================


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}
