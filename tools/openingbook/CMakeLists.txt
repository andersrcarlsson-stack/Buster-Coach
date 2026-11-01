# openingbook — prepares the opening data that ships with Buster Coach (not part of the app; run by hand).
#   cmake -S tools/openingbook -B tools/openingbook/build && cmake --build tools/openingbook/build
cmake_minimum_required(VERSION 3.16)
project(openingbook LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)       # millions of games: optimise by default
endif()
add_compile_options(-Wall -Wextra)

# The app's board model, built here too: the same SanToMove and FEN as the app (positions match exactly).
add_subdirectory(../../app/lib/model board_model EXCLUDE_FROM_ALL)

add_executable(openingbook main.cpp)
target_link_libraries(openingbook PRIVATE board_model)
