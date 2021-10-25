CMAKE_MINIMUM_REquired(VERSION 3.14 FATAL_ERROR)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(gd)
add_executable(app
               src/example.cpp)

set_target_properties(app
    PROPERTIES
      CXX_STANDARD 17
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
)
target_link_libraries(app PUBLIC gd)


# Debug version will have the symbols in the executables
# But allow release to be as small as possible while still having its symbols if needed
string(APPEND COLLECT_CMD
       "$<IF:$<CONFIG:Release>,"
          "objcopy;--only-keep-debug;$<TARGET_FILE_NAME:app>;$<TARGET_FILE_NAME:app>.sym,"
          "echo;collecting symobls skipped"
        ">")

string(APPEND STRIP_CMD
       "$<IF:$<CONFIG:Release>,"
          "objcopy;--strip-debug;$<TARGET_FILE_NAME:app>;$<TARGET_FILE_NAME:app>.rel,"
          "echo;stripping symobls skipped"
        ">")

message(STATUS "---> ${STRIP_EXE}")
add_custom_command(
  TARGET app POST_BUILD
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARTY_DIR}
  COMMAND "${STRIP_CMD}"
  COMMAND "${COLLECT_CMD}"
  COMMENT "Separate symobls from executable"
  # VERBATIM
  COMMAND_EXPAND_LISTS
)
