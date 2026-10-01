# Headless end-to-end check: play, die, reach game over, and persist the best score.
# Usage: cmake -DFLAPPY=<exe> -DBEST=<file> -P smoke_gameover.cmake
file(REMOVE "${BEST}")
set(ENV{SDL_VIDEO_DRIVER} dummy)
set(ENV{SDL_AUDIO_DRIVER} dummy)
execute_process(
    COMMAND "${FLAPPY}" --smoke 2400 --seed 1 --script die --best-file "${BEST}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out)
message(STATUS "${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "flappy exited with ${rc}")
endif()
if(NOT out MATCHES "state=3")
    message(FATAL_ERROR "run did not end in GameOver (state=3)")
endif()
if(NOT EXISTS "${BEST}")
    message(FATAL_ERROR "best-score file was not written: ${BEST}")
endif()
file(READ "${BEST}" best)
string(STRIP "${best}" best)
if(NOT best MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "best-score file holds '${best}', expected a positive score")
endif()
message(STATUS "best score persisted: ${best}")
