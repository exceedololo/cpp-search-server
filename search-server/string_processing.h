// в качестве заготовки кода используйте последнюю версию своей поисковой системы
#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <set>

std::vector<std::string_view> SplitIntoWords(std::string_view);

using TransparentStringSet = std::set<std::string, std::less<>>;

template <typename StringContainer>
TransparentStringSet MakeUniqueNonEmptyStrings(const StringContainer& strings){
    TransparentStringSet non_empty_strings;
    for (std::string_view str : strings){
        if (!str.empty()){
            non_empty_strings.emplace(str);
        }
    }
    return non_empty_strings;
}
