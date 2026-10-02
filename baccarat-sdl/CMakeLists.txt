cmake_minimum_required(VERSION 3.24)
project(baccarat-sdl CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BACCARAT_BUILD_GAME "Build the game and SDL dependencies" ON)
option(BACCARAT_BUILD_TESTS "Build tests" ON)

if(BACCARAT_BUILD_TESTS)
  enable_testing()
endif()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
include(BacTesting)

# Add rules subdirectory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/rules/CMakeLists.txt")
  add_subdirectory(src/rules)
endif()

# Add game subdirectory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/game/CMakeLists.txt")
  add_subdirectory(src/game)
endif()

# Add app subdirectory
if(BACCARAT_BUILD_GAME)
  add_subdirectory(src/app)

  # Smoke tests (headless with dummy drivers)
  if(BACCARAT_BUILD_TESTS)
    add_test(NAME smoke COMMAND baccarat --frames 120)
    add_test(NAME smoke_screenshot
      COMMAND baccarat --frames 30 --screenshot ${CMAKE_BINARY_DIR}/smoke.bmp)
    set_tests_properties(smoke smoke_screenshot PROPERTIES
      ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy"
      TIMEOUT 60)
  endif()
else()
  # When not building the game, still build app_options_tests
  if(BACCARAT_BUILD_TESTS)
    add_subdirectory(src/app)
  endif()
endif()
