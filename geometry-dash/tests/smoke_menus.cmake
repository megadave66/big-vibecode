# Headless menu smoke. Inputs: EXE, DATA, TMP.
set(SAVE "${TMP}/menus_progress.json")
file(REMOVE "${SAVE}")

# Keys land every 6 frames. Menu flow: Play, select level 2 (down is a row, right a column; here
# "right" moves to level 2), toggle practice with p, Enter plays, wait, Esc pauses, down+Enter =
# Restart, Esc pauses, three downs + Enter = Quit to menu, then click Play with the mouse.
set(SCRIPT "enter,right,p,enter,wait,wait,wait,esc,down,enter,wait,esc,down,down,down,enter,wait,click:640:367")
execute_process(
  COMMAND "${EXE}" --smoke 200 --mute --data "${DATA}" --save "${SAVE}" --menu-script "${SCRIPT}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "menu smoke exited with ${rc}")
endif()
if(NOT out MATCHES "state=LevelSelect")
  message(FATAL_ERROR "expected the script to end on the level select screen")
endif()
if(NOT EXISTS "${SAVE}")
  message(FATAL_ERROR "progress file was not written")
endif()
file(READ "${SAVE}" saved)
# Level 2, practice mode: two attempts (start + restart) must be saved.
string(JSON attempts GET "${saved}" levels 2 attempts)
if(attempts LESS 2)
  message(FATAL_ERROR "expected at least 2 attempts on level 2, got ${attempts}")
endif()

# A corrupt save must not crash the game.
file(WRITE "${SAVE}" "{ this is not json")
execute_process(
  COMMAND "${EXE}" --smoke 30 --mute --data "${DATA}" --save "${SAVE}" --menu-script "enter,esc"
  RESULT_VARIABLE rc2 OUTPUT_VARIABLE out2 ERROR_VARIABLE err2)
if(NOT rc2 EQUAL 0)
  message(FATAL_ERROR "corrupt-save run exited with ${rc2}: ${err2}")
endif()
