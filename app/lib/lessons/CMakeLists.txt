# The teaching levels' lesson moves (teaching_levels_design.md): plain C++ (no Qt), uses the model and the UCI
# scores, with its own test.
add_library(lessons STATIC lessons.cpp)
target_include_directories(lessons PUBLIC .)
target_link_libraries(lessons PUBLIC board_model uci_info)

add_executable(lessons_test lessons_test.cpp)
target_link_libraries(lessons_test PRIVATE lessons)
add_test(NAME lessons_tests COMMAND lessons_test)
