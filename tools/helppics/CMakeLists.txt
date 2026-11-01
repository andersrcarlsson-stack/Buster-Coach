# helppics — the Help's pictures and the coach's real sentences for them (not part of the app; run by hand).
#   cmake -S tools/helppics -B tools/helppics/build && cmake --build tools/helppics/build
# README.md: the four programs and how a picture gets into the Help.
cmake_minimum_required(VERSION 3.16)
project(helppics LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)
set(APP ${CMAKE_CURRENT_SOURCE_DIR}/../../app)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# The app's own code, built here too: the pictures are the real widgets, the words the real coach.
add_subdirectory(${APP}/lib/model board_model EXCLUDE_FROM_ALL)
add_subdirectory(${APP}/lib/uci uci_info EXCLUDE_FROM_ALL)
add_subdirectory(${APP}/lib/openings opening_names EXCLUDE_FROM_ALL)

# helppics: the pictures (the board, eval bar, graph, dot; the coach's cards drawn as coaching.cpp draws them).
add_executable(helppics helppics.cpp ${APP}/resources.qrc
    ${APP}/widgets/boardwidget.h ${APP}/widgets/boardwidget.cpp ${APP}/widgets/evalbar.h ${APP}/widgets/evalbar.cpp
    ${APP}/widgets/wingraph.h ${APP}/widgets/wingraph.cpp ${APP}/widgets/confidencedot.h ${APP}/widgets/confidencedot.cpp
    ${APP}/widgets/piecepainter.h ${APP}/widgets/piecepainter.cpp ${APP}/widgets/theme.h ${APP}/widgets/theme.cpp
    ${APP}/widgets/practicepanel.h ${APP}/widgets/practicepanel.cpp)
target_include_directories(helppics PRIVATE ${APP}/widgets)
target_link_libraries(helppics PRIVATE Qt6::Widgets board_model opening_names)
if(UNIX AND NOT APPLE)
    target_link_libraries(helppics PRIVATE xcb)          # theme.cpp's dark title bar (as in the app)
endif()

# coachtext: the real Coach's words for a position, from the built-in Buster.
add_executable(coachtext coachtext.cpp ${APP}/engine/coach.h ${APP}/engine/coach.cpp
    ${APP}/engine/engine.h ${APP}/engine/engine.cpp)
target_include_directories(coachtext PRIVATE ${APP}/engine)
target_compile_definitions(coachtext PRIVATE BUSTER="${APP}/build/buster")
target_link_libraries(coachtext PRIVATE Qt6::Core board_model uci_info)

# moves: every legal move of a position (for probe.py).
add_executable(moves moves.cpp)
target_link_libraries(moves PRIVATE board_model)

# page: a Help page rendered as a picture, to check its layout without the app.
add_executable(page page.cpp)
target_compile_definitions(page PRIVATE HELP_DIR="${APP}/help")
target_link_libraries(page PRIVATE Qt6::Widgets)

# bookinfo: the book's moves and the names after them, with the practice rule (inside or leaving).
add_executable(bookinfo bookinfo.cpp)
target_compile_definitions(bookinfo PRIVATE DATA_DIR="${APP}/data")
target_link_libraries(bookinfo PRIVATE opening_names board_model)
