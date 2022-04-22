cmake_minimum_required(VERSION 3.22)
project(8fin)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -ltbb -lpthread")
add_executable(${PROJECT_NAME}
        main.cpp
        document.cpp
        process_queries.cpp
        read_input_functions.cpp
        request_queue.cpp
        search_server.cpp
        string_processing.cpp
        test_example_functions.cpp
        )

target_link_libraries(${PROJECT_NAME} PRIVATE tbb)
