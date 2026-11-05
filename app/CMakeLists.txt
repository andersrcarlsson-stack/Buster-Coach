cmake_minimum_required(VERSION 3.16)
project(buster_gui VERSION 1.0.1 LANGUAGES CXX)   # the one place the version is set (release_design.md 13)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # build/compile_commands.json: every file's exact compile command (IntelliSense)
# No build type given (a fresh clone, the release workflow): Release, optimised. Unoptimised, reading saved games was
# ~10x slower. A type given on the command line (-DCMAKE_BUILD_TYPE=Debug) still wins.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()


add_compile_options(-Wall -Wextra) # warnings for every target defined below (gcc/clang flags)

# All programs (the app and the tests) land directly in build/, wherever their sources live:
# ./build/buster_gui, ./build/model_test, ...
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# The folders:
#   lib/      plain C++ libraries (no Qt), each with its own test: model, uci, clock, record
#   engine/   the engines over UCI (Engine) and the two coaches (Coach, MasterCoach)
#   game/     Game: the hub and the one source of truth
#   widgets/  what you see: the board, the board area, the eval bar, the player strips
#   window/   MainWindow: builds the window and wires the parts together
#   main.cpp  creates the application and the window

# The test gate (ctest --test-dir build). Before add_subdirectory, so the libraries' add_test calls count.
enable_testing()
add_subdirectory(lib/model)
add_subdirectory(lib/uci)
add_subdirectory(lib/clock)
add_subdirectory(lib/record)
add_subdirectory(lib/openings)
add_subdirectory(lib/pgn)


# Qt's paths, flags and libraries come from here (installed Qt 6.4, Widgets module).
find_package(Qt6 REQUIRED COMPONENTS Widgets)
# Run moc on every header with Q_OBJECT. Without it: link error "undefined reference to vtable for ...".
set(CMAKE_AUTOMOC ON)
# Run rcc on .qrc files: their contents are compiled into the program, reachable as ":/..." paths.
set(CMAKE_AUTORCC ON)
# Headers are listed too, so AUTOMOC sees them.
add_executable(buster_gui
    main.cpp
    resources.qrc help/help.qrc
    engine/engine.h engine/engine.cpp engine/coach.h engine/coach.cpp engine/mastercoach.h engine/mastercoach.cpp 
    engine/enginetester.h engine/enginetester.cpp
    game/game.h game/game.cpp
    widgets/boardwidget.h widgets/boardwidget.cpp widgets/boardarea.h widgets/boardarea.cpp
    widgets/evalbar.h widgets/evalbar.cpp widgets/playerstrip.h widgets/playerstrip.cpp
    widgets/piecepainter.h widgets/piecepainter.cpp widgets/confidencedot.h widgets/confidencedot.cpp
    widgets/capturedrow.h widgets/capturedrow.cpp widgets/theme.h widgets/theme.cpp
    widgets/editorpanel.h widgets/editorpanel.cpp widgets/enginepanel.h widgets/enginepanel.cpp
    widgets/wingraph.h widgets/wingraph.cpp widgets/armedbutton.h widgets/armedbutton.cpp
    widgets/practicepanel.h widgets/practicepanel.cpp widgets/gamespanel.h widgets/gamespanel.cpp
    window/mainwindow.h window/mainwindow.cpp window/menus.cpp window/hint_threat.cpp window/coaching.cpp
    window/panel.cpp window/logs.cpp window/editor.cpp window/engines.cpp window/practice.cpp window/saving.cpp
    window/help.cpp)
# The app's own folders: an #include "coach.h" finds the header wherever it lives.
target_include_directories(buster_gui PRIVATE engine game widgets window)
# Windows: a GUI program, no console window beside it (WinMain via Qt). Elsewhere this property does nothing.
set_target_properties(buster_gui PROPERTIES WIN32_EXECUTABLE ON)
# The version, for the code: a -D define (About, the window's application version).
target_compile_definitions(buster_gui PRIVATE BUSTER_COACH_VERSION="${PROJECT_VERSION}")
target_link_libraries(buster_gui PRIVATE Qt6::Widgets board_model uci_info chess_clock game_record opening_names game_pgn)
# The title bar's dark hint talks to X11 directly (theme.cpp): link the xcb library (Linux only).
if(UNIX AND NOT APPLE)
    target_link_libraries(buster_gui PRIVATE xcb)
endif()

# The engine tests from the command line (engine_design.md): ./build/engine_check <engine file>
add_executable(engine_check engine/engine_check.cpp engine/enginetester.h engine/enginetester.cpp
    engine/engine.h engine/engine.cpp)
target_include_directories(engine_check PRIVATE engine)
target_link_libraries(engine_check PRIVATE Qt6::Core uci_info)

