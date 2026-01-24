cmake_minimum_required(VERSION 3.9)
project(HelloLog CXX)
set(CMAKE_CXX_STANDARD 20)

add_executable(hello_log
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
    src/logger.h
    src/logging_request_handler.h
)
# используем "импортированную" цель CONAN_PKG::boost
target_include_directories(hello_log PRIVATE CONAN_PKG::boost)
target_link_libraries(hello_log PRIVATE)
