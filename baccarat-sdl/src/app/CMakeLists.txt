# SDL3 executable `baccarat` (only when BACCARAT_BUILD_GAME=ON).
if(BACCARAT_BUILD_GAME)
  file(GLOB BAC_APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
  add_executable(baccarat ${BAC_APP_SOURCES})
  target_include_directories(baccarat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(baccarat PRIVATE cxx_std_20)
  target_link_libraries(baccarat PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_mixer::SDL3_mixer)
  if(TARGET baccarat_game)
    target_link_libraries(baccarat PRIVATE baccarat_game)
  endif()
  bac_set_warnings(baccarat)

  if(BACCARAT_BUILD_TESTS)
    # Scripted flows (headless, deterministic).
    set(BAC_SCRIPT_ENV "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy")
    add_test(NAME script_round COMMAND baccarat --seed 7 --script
      "chip:25,bet:player,bet:banker,deal,wait:400,assert-phase:betting,quit")
    # $900 on Player, $75 on Banker (bankroll 2500 left), then $100 chip -> all-in $25, then insufficient.
    add_test(NAME script_funds COMMAND baccarat --seed 7 --script
      "chip:100,bet:player,bet:player,bet:player,bet:player,bet:player,bet:player,bet:player,bet:player,bet:player,assert-bankroll:10000,chip:25,bet:banker,bet:banker,bet:banker,assert-bankroll:2500,chip:100,bet:tie,assert-bankroll:0,bet:tie,assert-bankroll:0,remove:tie,assert-bankroll:2500,quit")
    add_test(NAME script_bad_assert COMMAND baccarat --script "assert-bankroll:1,quit")
    set_tests_properties(script_bad_assert PROPERTIES WILL_FAIL TRUE)
    add_test(NAME script_many_rounds COMMAND baccarat --seed 3 --script
      "chip:5,bet:player,deal,next,next,next,assert-phase:betting,rebet,deal,next,next,next,assert-phase:betting,rebet,deal,next,next,next,assert-phase:betting,quit")
    set_tests_properties(script_round script_funds script_bad_assert script_many_rounds PROPERTIES
      ENVIRONMENT "${BAC_SCRIPT_ENV}" TIMEOUT 60)
  endif()

  set(BAC_ASSETS_SRC "${PROJECT_SOURCE_DIR}/assets")
  target_compile_definitions(baccarat PRIVATE BAC_SOURCE_ASSETS_DIR="${BAC_ASSETS_SRC}")
  # Copy assets next to the executable so it runs from any working directory.
  add_custom_command(TARGET baccarat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${BAC_ASSETS_SRC}" "$<TARGET_FILE_DIR:baccarat>/assets"
    COMMENT "Copying assets next to baccarat")
endif()

# Pure-logic app tests (no SDL needed).
if(BACCARAT_BUILD_TESTS)
  bac_add_doctest(app_options_tests
    SOURCES Options.cpp tests/test_options.cpp
    LIBS)
  target_include_directories(app_options_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  # SDL-free UI helpers: layout/hit-testing, money formatting, deal timeline.
  bac_add_doctest(app_ui_tests
    SOURCES Layout.cpp Format.cpp DealAnimator.cpp tests/test_ui.cpp
    LIBS baccarat_rules)
  target_include_directories(app_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
