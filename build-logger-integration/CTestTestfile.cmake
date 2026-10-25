# CMake generated Testfile for 
# Source directory: /home/lhjwsl/aster_server
# Build directory: /home/lhjwsl/aster_server/build-logger-integration
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(reactor_integration "/usr/bin/python3" "/home/lhjwsl/aster_server/tests/integration.py" "/home/lhjwsl/aster_server/build-logger-integration/aster_server")
set_tests_properties(reactor_integration PROPERTIES  TIMEOUT "45" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;33;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(event_loop_callbacks "/home/lhjwsl/aster_server/build-logger-integration/event_loop_test")
set_tests_properties(event_loop_callbacks PROPERTIES  TIMEOUT "5" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;38;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(connection_lifecycle "/home/lhjwsl/aster_server/build-logger-integration/lifecycle_test")
set_tests_properties(connection_lifecycle PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;47;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(backpressure_1 "/home/lhjwsl/aster_server/build-logger-integration/backpressure_test" "1")
set_tests_properties(backpressure_1 PROPERTIES  TIMEOUT "15" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;57;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(backpressure_2 "/home/lhjwsl/aster_server/build-logger-integration/backpressure_test" "2")
set_tests_properties(backpressure_2 PROPERTIES  TIMEOUT "15" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;57;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(backpressure_3 "/home/lhjwsl/aster_server/build-logger-integration/backpressure_test" "3")
set_tests_properties(backpressure_3 PROPERTIES  TIMEOUT "15" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;57;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(backpressure_4 "/home/lhjwsl/aster_server/build-logger-integration/backpressure_test" "4")
set_tests_properties(backpressure_4 PROPERTIES  TIMEOUT "15" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;57;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(logger_unit "/home/lhjwsl/aster_server/build-logger-integration/logger_test")
set_tests_properties(logger_unit PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;66;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
add_test(logger_shutdown "/usr/bin/python3" "/home/lhjwsl/aster_server/tests/logger_shutdown.py" "/home/lhjwsl/aster_server/build-logger-integration/aster_server")
set_tests_properties(logger_shutdown PROPERTIES  TIMEOUT "20" _BACKTRACE_TRIPLES "/home/lhjwsl/aster_server/CMakeLists.txt;68;add_test;/home/lhjwsl/aster_server/CMakeLists.txt;0;")
