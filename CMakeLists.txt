cmake_minimum_required(VERSION 3.16)

project(AsterServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

option(ASTER_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ASTER_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "ASTER_SANITIZERS requires GCC or Clang")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_executable(aster_server src/main.cpp src/Epoll.cpp src/EventLoop.cpp src/Channel.cpp src/TcpServer.cpp src/TcpConnection.cpp src/setNotBlocking.cpp)

target_include_directories(aster_server
    PRIVATE ${PROJECT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_sources(aster_server PRIVATE src/Logger.cpp)
target_link_libraries(aster_server PRIVATE Threads::Threads)

include(CTest)
if(BUILD_TESTING)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME reactor_integration COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/integration.py $<TARGET_FILE:aster_server>)
set_tests_properties(reactor_integration PROPERTIES TIMEOUT 45)

add_executable(event_loop_test tests/event_loop_test.cpp src/EventLoop.cpp src/Epoll.cpp src/Channel.cpp)
target_include_directories(event_loop_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME event_loop_callbacks COMMAND event_loop_test)
set_tests_properties(event_loop_callbacks PROPERTIES TIMEOUT 5)
endif()

if(BUILD_TESTING)
    add_executable(lifecycle_test tests/lifecycle_test.cpp src/TcpServer.cpp src/TcpConnection.cpp src/EventLoop.cpp src/Epoll.cpp src/Channel.cpp src/setNotBlocking.cpp)
    target_sources(lifecycle_test PRIVATE src/Logger.cpp)
    target_link_libraries(lifecycle_test PRIVATE Threads::Threads)
    target_include_directories(lifecycle_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
    add_test(NAME connection_lifecycle COMMAND lifecycle_test)
    set_tests_properties(connection_lifecycle PROPERTIES TIMEOUT 10)
endif()

if(BUILD_TESTING)
    add_executable(backpressure_test tests/backpressure_test.cpp src/TcpConnection.cpp src/EventLoop.cpp src/Epoll.cpp src/Channel.cpp)
    target_sources(backpressure_test PRIVATE src/Logger.cpp)
    target_link_libraries(backpressure_test PRIVATE Threads::Threads)
    target_include_directories(backpressure_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
    foreach(case RANGE 1 4)
        add_test(NAME backpressure_${case} COMMAND backpressure_test ${case})
        set_tests_properties(backpressure_${case} PROPERTIES TIMEOUT 15)
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(logger_test src/demo/logger_test.cpp src/Logger.cpp)
    target_include_directories(logger_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(logger_test PRIVATE Threads::Threads)
    add_test(NAME logger_unit COMMAND logger_test)
    set_tests_properties(logger_unit PROPERTIES TIMEOUT 10)
    add_test(NAME logger_shutdown COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/logger_shutdown.py $<TARGET_FILE:aster_server>)
    set_tests_properties(logger_shutdown PROPERTIES TIMEOUT 20)
endif()
