cmake_minimum_required(VERSION 3.16)

project(AsterServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(aster_server src/main.cpp src/Epoll.cpp src/EventLoop.cpp src/Channel.cpp src/TcpServer.cpp src/TcpConnection.cpp src/setNotBlocking.cpp)

target_include_directories(aster_server
    PRIVATE ${PROJECT_SOURCE_DIR}/include
)

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
