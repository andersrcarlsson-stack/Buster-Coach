# The board model: plain C++ (no Qt), built once as a library. Test: perft and the model's rules.
add_library(board_model STATIC position.cpp fen.cpp move.cpp test.cpp)
target_include_directories(board_model PUBLIC .)

add_executable(model_test model_test.cpp)
target_link_libraries(model_test PRIVATE board_model)
add_test(NAME model_tests COMMAND model_test)
