# Chess clocks: plain C++ (no Qt), with its own test.
add_library(chess_clock STATIC chessclock.cpp)
target_include_directories(chess_clock PUBLIC .)

add_executable(clock_test clock_test.cpp)
target_link_libraries(clock_test PRIVATE chess_clock)
add_test(NAME clock_tests COMMAND clock_test)
