# The opening names: plain C++ (no Qt), uses the board model for the key, with its own test.
add_library(opening_names STATIC openings.cpp)
target_include_directories(opening_names PUBLIC .)
target_link_libraries(opening_names PUBLIC board_model)

add_executable(openings_test openings_test.cpp)
target_link_libraries(openings_test PRIVATE opening_names)
# The test reads the real table from the source tree: its path as a macro.
target_compile_definitions(openings_test PRIVATE OPENINGS_TSV="${CMAKE_SOURCE_DIR}/data/openings.tsv"
                                                 OPENINGS_BOOK="${CMAKE_SOURCE_DIR}/data/book.bin")
add_test(NAME openings_tests COMMAND openings_test)
