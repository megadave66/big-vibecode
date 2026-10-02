cmake_minimum_required(VERSION 3.24)
project(GeometryDash LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
# CMake 4 drops support for very old cmake_minimum_required values used by some deps.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
# We call FetchContent_Populate on purpose (to patch SDL_ttf/SDL_mixer sources before
# add_subdirectory). Keep the old behaviour without the deprecation warning.
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()
set(FETCHCONTENT_QUIET OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------- dependencies
# All dependencies come from release tarballs (no git clone).
FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
  URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(SDL3_ttf
  URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
  URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# The SDL_ttf release tarball ships empty external/ folders (git submodules).
# We fetch the SDL-patched FreeType as a tarball and copy it in, so SDLTTF_VENDORED works.
FetchContent_Declare(sdl_freetype
  URL https://github.com/libsdl-org/freetype/archive/9973564cfa63763a3e4ac67c09147899539b1e07.tar.gz  # VER-2-13-2-SDL branch, pinned commit
  URL_HASH SHA256=026a05a49d114a1235d2926f4c03a9330e4b1a6efe7c217ec9607904c32907d4
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(SDL3_mixer
  URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz
  URL_HASH SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
  URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# SDL3 (static)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# SDL3_ttf (static, vendored FreeType only; no HarfBuzz / color emoji needed)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(SDL3_ttf)
if(NOT sdl3_ttf_POPULATED)
  FetchContent_Populate(SDL3_ttf)
endif()
FetchContent_GetProperties(sdl_freetype)
if(NOT sdl_freetype_POPULATED)
  FetchContent_Populate(sdl_freetype)
endif()
if(NOT EXISTS "${sdl3_ttf_SOURCE_DIR}/external/freetype/CMakeLists.txt")
  file(COPY "${sdl_freetype_SOURCE_DIR}/" DESTINATION "${sdl3_ttf_SOURCE_DIR}/external/freetype")
endif()
add_subdirectory("${sdl3_ttf_SOURCE_DIR}" "${sdl3_ttf_BINARY_DIR}" EXCLUDE_FROM_ALL)

# SDL3_mixer (static; WAV + Ogg Vorbis via bundled stb_vorbis only, everything else off)
set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVE ON CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
foreach(_opt AIFF VOC AU FLAC GME MOD MP3 MIDI OPUS WAVPACK)
  set(SDLMIXER_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_GetProperties(SDL3_mixer)
if(NOT sdl3_mixer_POPULATED)
  FetchContent_Populate(SDL3_mixer)
endif()
add_subdirectory("${sdl3_mixer_SOURCE_DIR}" "${sdl3_mixer_BINARY_DIR}" EXCLUDE_FROM_ALL)

# doctest / nlohmann_json (header-only; no sub-builds)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest nlohmann_json)

# ---------------------------------------------------------------- our targets
set(GD_WARN -Wall -Wextra)
set(GD_DATA_ROOT "${CMAKE_SOURCE_DIR}")

file(GLOB GD_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/core/*.cpp")
add_library(gd_core STATIC ${GD_CORE_SOURCES})
target_include_directories(gd_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(gd_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(gd_core PUBLIC GD_DATA_DIR="${GD_DATA_ROOT}")
target_compile_options(gd_core PRIVATE ${GD_WARN})

file(GLOB GD_APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/app/*.cpp")
add_executable(geometry_dash ${GD_APP_SOURCES})
target_include_directories(geometry_dash PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src/app")
target_link_libraries(geometry_dash PRIVATE gd_core SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_mixer::SDL3_mixer)
target_compile_options(geometry_dash PRIVATE ${GD_WARN})

file(GLOB GD_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(gd_tests ${GD_TEST_SOURCES})
target_link_libraries(gd_tests PRIVATE gd_core doctest::doctest)
target_compile_options(gd_tests PRIVATE ${GD_WARN})

foreach(_tool gd_validate gd_replay gd_solve)
  if(EXISTS "${CMAKE_SOURCE_DIR}/tools/${_tool}.cpp")
    add_executable(${_tool} "${CMAKE_SOURCE_DIR}/tools/${_tool}.cpp")
    target_link_libraries(${_tool} PRIVATE gd_core)
    target_compile_options(${_tool} PRIVATE ${GD_WARN})
  endif()
endforeach()

# ---------------------------------------------------------------- ctest
enable_testing()
add_test(NAME unit COMMAND gd_tests)
set(GD_TEST_TMP "${CMAKE_BINARY_DIR}/test_tmp")
file(MAKE_DIRECTORY "${GD_TEST_TMP}")
add_test(NAME smoke COMMAND geometry_dash --smoke 300 --mute --data "${GD_DATA_ROOT}" --save "${GD_TEST_TMP}/smoke_progress.json")
set_tests_properties(smoke PROPERTIES
  ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy" TIMEOUT 60)
# Smoke run that plays a whole level in fly mode (real level 1 when it exists, else the sample).
if(EXISTS "${CMAKE_SOURCE_DIR}/levels/level01.json")
  set(GD_SMOKE_LEVEL_ARGS --level 1)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/levels/_sample.json")
  set(GD_SMOKE_LEVEL_ARGS --level-file "${CMAKE_SOURCE_DIR}/levels/_sample.json")
endif()
if(DEFINED GD_SMOKE_LEVEL_ARGS)
  add_test(NAME smoke_game COMMAND geometry_dash --smoke 2800 ${GD_SMOKE_LEVEL_ARGS} --mute --data "${GD_DATA_ROOT}" --save "${GD_TEST_TMP}/smoke_game_progress.json")
  set_tests_properties(smoke_game PROPERTIES
    ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy" TIMEOUT 120)
endif()

# Menu walk with scripted keys and mouse: main -> level select -> practice on -> play level 1 -> pause ->
# restart -> resume -> pause -> quit to menu. Then a second run loads the file it wrote.
add_test(NAME smoke_menus COMMAND ${CMAKE_COMMAND}
  -DEXE=$<TARGET_FILE:geometry_dash> -DDATA=${GD_DATA_ROOT} -DTMP=${GD_TEST_TMP}
  -P "${CMAKE_SOURCE_DIR}/tests/smoke_menus.cmake")
set_tests_properties(smoke_menus PROPERTIES
  ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy" TIMEOUT 60)

file(GLOB GD_LEVEL_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/levels/level[0-9][0-9].json")
foreach(_f ${GD_LEVEL_FILES})
  get_filename_component(_n "${_f}" NAME_WE)
  if(TARGET gd_validate)
    add_test(NAME validate_${_n} COMMAND gd_validate "${_f}" --data "${GD_DATA_ROOT}")
  endif()
endforeach()

file(GLOB GD_REPLAY_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/replays/level[0-9][0-9].replay")
foreach(_f ${GD_REPLAY_FILES})
  get_filename_component(_n "${_f}" NAME_WE)
  if(TARGET gd_replay)
    add_test(NAME replay_${_n} COMMAND gd_replay "${CMAKE_SOURCE_DIR}/levels/${_n}.json" "${_f}" --data "${GD_DATA_ROOT}")
  endif()
endforeach()
