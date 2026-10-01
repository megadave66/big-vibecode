add_library(baccarat_rules STATIC
  card.cpp
  draw_rules.cpp
  shoe.cpp
  round.cpp
  settlement.cpp
)
target_include_directories(baccarat_rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(baccarat_rules PUBLIC cxx_std_20)
if(COMMAND bac_set_warnings)
  bac_set_warnings(baccarat_rules)
endif()

if(BACCARAT_BUILD_TESTS AND COMMAND bac_add_doctest)
  bac_add_doctest(rules_tests
    SOURCES
      tests/test_card.cpp
      tests/test_draw_rules.cpp
      tests/test_shoe.cpp
      tests/test_round.cpp
      tests/test_settlement.cpp
    LIBS baccarat_rules
  )
endif()
