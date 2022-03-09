// в качестве заготовки кода используйте последнюю версию своей поисковой системы
#include "log_duration.h"

#include <chrono>
#include <iostream>


LogDuration(const std::string& text, std::ostream& output = std::cerr) 
    : text_(text)
, output_(output) {
}

~LogDuration() {
    using namespace std::chrono;
    using namespace std::literals;

    const auto end_time = Clock::now();
    const auto dur = end_time - start_time_;
    std::cerr << text_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
}
