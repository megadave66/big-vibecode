add_library(baccarat_game STATIC
  session.cpp
  script.cpp
)
target_include_directories(baccarat_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(baccarat_game PUBLIC cxx_std_20)
target_link_libraries(baccarat_game PUBLIC baccarat_rules)
if(COMMAND bac_set_warnings)
  bac_set_warnings(baccarat_game)
endif()

if(BACCARAT_BUILD_TESTS AND COMMAND bac_add_doctest)
  bac_add_doctest(game_tests
    SOURCES
      tests/test_session.cpp
      tests/test_script.cpp
    LIBS baccarat_game
  )
endif()
