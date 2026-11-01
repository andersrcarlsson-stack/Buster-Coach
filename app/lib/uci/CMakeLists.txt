# UCI text parsing and the move verdicts: plain C++ (no Qt), with its own test.
add_library(uci_info STATIC uci_info.cpp)
target_include_directories(uci_info PUBLIC .)

add_executable(uci_test uci_test.cpp)
target_link_libraries(uci_test PRIVATE uci_info)
add_test(NAME uci_tests COMMAND uci_test)
