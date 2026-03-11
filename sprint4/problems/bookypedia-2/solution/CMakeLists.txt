cmake_minimum_required(VERSION 3.11)

project(bookypedia CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(libbookypedia STATIC
	src/app/unit_of_work.h 
	src/app/use_cases_impl.cpp 
	src/app/use_cases_impl.h 
	src/app/use_cases.h 
	src/domain/author_fwd.h 
	src/domain/author.cpp 
	src/domain/author.h 
	src/domain/book_fwd.h 
	src/domain/book.cpp 
	src/domain/book.h 
	src/menu/menu.cpp 
	src/menu/menu.h 
	src/postgres/postgres.cpp 
	src/postgres/postgres.h 
	src/postgres/unit_of_work_impl.cpp 
	src/postgres/unit_of_work_impl.h 
	src/ui/view.cpp 
	src/ui/view.h 
	src/util/tag_util.h 
	src/util/tagged_uuid.cpp 
	src/util/tagged_uuid.h 
	src/util/tagged.h 
	src/bookypedia.cpp 
	src/bookypedia.h 
	src/main.cpp
)

target_link_libraries(libbookypedia PUBLIC CONAN_PKG::boost Threads::Threads CONAN_PKG::libpq CONAN_PKG::libpqxx)

add_executable(bookypedia
	src/bookypedia.cpp
	src/bookypedia.h
	src/main.cpp
)
target_link_libraries(bookypedia PRIVATE CONAN_PKG::boost libbookypedia)

add_executable(tests
	tests/use_case_tests.cpp
	tests/tagged_uuid_tests.cpp
)
target_link_libraries(tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::gtest libbookypedia)
