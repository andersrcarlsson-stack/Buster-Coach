# The game record: plain C++ (no Qt), uses uci_info's types, with its own test.
add_library(game_record STATIC gamerecord.cpp)
target_include_directories(game_record PUBLIC .)
target_link_libraries(game_record PUBLIC uci_info)

add_executable(record_test record_test.cpp)
target_link_libraries(record_test PRIVATE game_record)
add_test(NAME record_tests COMMAND record_test)
