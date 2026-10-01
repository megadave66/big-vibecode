# Third-party dependencies, pinned to official release archives from GitHub.
# Release archives (URL + SHA256) are used instead of git clones: they are
# smaller, faster and reproducible.
include(FetchContent)

# CMake 4: download timestamps = extraction time (avoids needless rebuilds).
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
  doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
  URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

if(BACCARAT_BUILD_GAME)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

  # --- SDL3 -----------------------------------------------------------------
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
  )
  FetchContent_MakeAvailable(SDL3)

  # --- SDL_ttf (vendored FreeType only; no HarfBuzz / PlutoSVG) -------------
  # The release archive does not contain the vendored submodules, so we fetch
  # SDL's FreeType fork (the exact commit SDL_ttf 3.2.2 pins) into
  # external/freetype ourselves, then add SDL_ttf as a subdirectory.
  set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
  set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
  set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
  set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL_ttf
    URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
    URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
    SOURCE_SUBDIR bac-populate-only
  )
  FetchContent_MakeAvailable(SDL_ttf)
  FetchContent_Declare(
    sdlttf_freetype
    URL https://github.com/libsdl-org/freetype/archive/9973564cfa63763a3e4ac67c09147899539b1e07.tar.gz
    URL_HASH SHA256=026a05a49d114a1235d2926f4c03a9330e4b1a6efe7c217ec9607904c32907d4
    SOURCE_DIR ${sdl_ttf_SOURCE_DIR}/external/freetype
    SOURCE_SUBDIR bac-populate-only
  )
  FetchContent_MakeAvailable(sdlttf_freetype)
  add_subdirectory(${sdl_ttf_SOURCE_DIR} ${sdl_ttf_BINARY_DIR} EXCLUDE_FROM_ALL)

  # --- SDL_mixer (WAV + OGG via built-in stb_vorbis only) -------------------
  set(SDLMIXER_VENDORED OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVE ON CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
  foreach(opt AIFF VOC AU FLAC GME MOD MP3 MIDI OPUS VORBIS_VORBISFILE VORBIS_TREMOR WAVPACK)
    set(SDLMIXER_${opt} OFF CACHE BOOL "" FORCE)
  endforeach()
  FetchContent_Declare(
    SDL_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz
    URL_HASH SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
  )
  FetchContent_MakeAvailable(SDL_mixer)
endif()
