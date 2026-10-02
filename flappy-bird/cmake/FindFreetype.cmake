# Shim: SDL3_ttf calls find_package(Freetype REQUIRED) when SDLTTF_VENDORED is
# OFF. The top-level CMakeLists builds FreeType from a pinned tarball and
# provides Freetype::Freetype before SDL3_ttf is configured, so just report it.
if(TARGET Freetype::Freetype)
    set(Freetype_FOUND TRUE)
    set(FREETYPE_FOUND TRUE)
else()
    set(Freetype_FOUND FALSE)
    set(FREETYPE_FOUND FALSE)
endif()
