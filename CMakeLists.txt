cmake_minimum_required(VERSION 3.16)

project(AsterServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(aster_server src/main.cpp)

target_include_directories(aster_server
    PRIVATE ${PROJECT_SOURCE_DIR}/include
)

enable_testing()
add_test(NAME server_startup COMMAND aster_server)
