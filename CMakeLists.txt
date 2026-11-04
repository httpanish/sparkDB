cmake_minimum_required(VERSION 3.16)
project(sparkdb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(sparkdb_core src/KvStore.cpp)
target_include_directories(sparkdb_core PUBLIC src)
target_compile_options(sparkdb_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(sparkdb src/main.cpp)
target_link_libraries(sparkdb PRIVATE sparkdb_core)
target_compile_options(sparkdb PRIVATE -Wall -Wextra -Wpedantic)

include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

enable_testing()
add_executable(sparkdb_tests tests/KvStoreTest.cpp)
target_link_libraries(sparkdb_tests PRIVATE sparkdb_core Catch2::Catch2WithMain)
target_compile_options(sparkdb_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sparkdb_tests COMMAND sparkdb_tests)
