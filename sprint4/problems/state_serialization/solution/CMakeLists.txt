cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

get_property(importTargets DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY IMPORTED_TARGETS)
message(STATUS "${importTargets}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(game_lib STATIC
    src/model.h
    src/model.cpp
    src/tagged.h
    src/loot_generator.h
    src/loot_generator.cpp
    src/logger.h
    src/file_handler.h
    src/file_handler.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/boost_json.cpp
    src/collision_detector.h
    src/collision_detector.cpp
    src/state_serialization.h
)

target_include_directories(game_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    CONAN_PKG::boost
)

target_link_libraries(game_lib PUBLIC 
    CONAN_PKG::boost
    Threads::Threads
)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/request_handler.h
    src/logging_request_handler.h
    src/ticker.h
)

target_include_directories(game_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    CONAN_PKG::boost
)

target_link_libraries(game_server 
    game_lib
    CONAN_PKG::boost
    Threads::Threads
)

add_executable(game_server_tests
    src/model.h
    src/model.cpp
    src/json_loader.h
    src/json_loader.cpp
    tests/loot_generator_tests.cpp
    tests/test_player.cpp
    tests/test_json_loader.cpp
)

target_include_directories(game_server_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    CONAN_PKG::boost
)

target_link_libraries(game_server_tests 
    game_lib
    CONAN_PKG::catch2
    CONAN_PKG::boost
    Threads::Threads
)