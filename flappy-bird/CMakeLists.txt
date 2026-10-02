cmake_minimum_required(VERSION 3.25)
project(flappy_remake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Old third-party CMake files declare ancient minimum versions.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

option(FLAPPY_WERROR "Treat warnings in our own targets as errors" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# --- Dependencies (URL tarballs only, pinned by SHA256) -----------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# FreeType (SDL_ttf release tarballs do not ship their external/ sources)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
    URL https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz
    URL_HASH SHA256=0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# SDL3_ttf
set(SDLTTF_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_ttf
    URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
    URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# SDL3_mixer: only built-in decoders (WAV/AIFF/VOC/AU, stb_vorbis OGG, dr_mp3, dr_flac)
set(SDLMIXER_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC_LIBFLAC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD_XMP OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI_TIMIDITY OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_DRMP3 ON CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz
    URL_HASH SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# doctest
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
    URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

FetchContent_MakeAvailable(SDL3)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)  # FreeType 2.13.3 asks for an old CMake minimum
FetchContent_MakeAvailable(freetype)
unset(CMAKE_POLICY_VERSION_MINIMUM)
# FreeType's bundled gzip code redefines OS_CODE; harmless, silence it so the build is warning-free.
target_compile_options(freetype PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang,GNU>:-w>)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

FetchContent_MakeAvailable(SDL3_ttf SDL3_mixer doctest)

# --- Warnings for OUR targets only ------------------------------------------
function(flappy_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(FLAPPY_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
        if(FLAPPY_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# --- Targets -----------------------------------------------------------------
file(GLOB FLAPPY_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp)
add_library(flappy_core STATIC ${FLAPPY_CORE_SOURCES})
target_include_directories(flappy_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(flappy_core PUBLIC cxx_std_20)
flappy_set_warnings(flappy_core)

file(GLOB FLAPPY_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp)
add_executable(flappy ${FLAPPY_APP_SOURCES})
target_link_libraries(flappy PRIVATE flappy_core SDL3_ttf::SDL3_ttf SDL3_mixer::SDL3_mixer SDL3::SDL3)
flappy_set_warnings(flappy)
add_custom_command(TARGET flappy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:flappy>/assets
    COMMENT "Copying assets next to the flappy binary")

enable_testing()

file(GLOB FLAPPY_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
add_executable(flappy_tests ${FLAPPY_TEST_SOURCES})
target_link_libraries(flappy_tests PRIVATE flappy_core doctest::doctest)
flappy_set_warnings(flappy_tests)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(flappy_tests)

set(FLAPPY_DUMMY_ENV "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy")

add_test(NAME smoke
    COMMAND flappy --smoke 1200 --seed 1 --best-file ${CMAKE_BINARY_DIR}/smoke_best.txt)
set_tests_properties(smoke PROPERTIES ENVIRONMENT "${FLAPPY_DUMMY_ENV}")

add_test(NAME screenshot
    COMMAND flappy --screenshot ${CMAKE_BINARY_DIR}/shot_play.png --frames 400 --seed 1
            --best-file ${CMAKE_BINARY_DIR}/shot_best.txt)
set_tests_properties(screenshot PROPERTIES ENVIRONMENT "${FLAPPY_DUMMY_ENV}")

add_test(NAME smoke_gameover
    COMMAND ${CMAKE_COMMAND} -DFLAPPY=$<TARGET_FILE:flappy>
            -DBEST=${CMAKE_BINARY_DIR}/gameover_best.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/smoke_gameover.cmake)
