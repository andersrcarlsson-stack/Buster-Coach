# PGN: games as text, out and in (pgn_design.md). Plain C++ (no Qt), uses the game record; with its own test.
add_library(game_pgn STATIC pgn.cpp)
target_include_directories(game_pgn PUBLIC .)
target_link_libraries(game_pgn PUBLIC game_record board_model)   # the model: move numbers, "Best was" in SAN

add_executable(pgn_test pgn_test.cpp)
target_link_libraries(pgn_test PRIVATE game_pgn)
add_test(NAME pgn_tests COMMAND pgn_test)
# The real files (lichess's CC0 database, and games Buster Coach saved): their folder as a macro.
target_compile_definitions(pgn_test PRIVATE PGN_TESTDATA="${CMAKE_CURRENT_SOURCE_DIR}/testdata")

