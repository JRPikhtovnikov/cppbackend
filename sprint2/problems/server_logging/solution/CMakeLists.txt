cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Conan 1 (generator "cmake") -> conanbuildinfo.cmake + переменная CONAN_LIBS
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.h
    src/file_handler.h
    src/file_handler.cpp
    src/logging_request_handler.h
    src/logger.h
)

# Сначала системные штуки
target_link_libraries(game_server PRIVATE
    Threads::Threads
    dl
)

# Для статических архивов Conan/Boost: несколько проходов по .a, чтобы не было undefined reference
target_link_options(game_server PRIVATE "-Wl,--start-group")
target_link_libraries(game_server PRIVATE ${CONAN_LIBS})
target_link_options(game_server PRIVATE "-Wl,--end-group")
