CMAKE_MINIMUM_REquired(VERSION 3.14 FATAL_ERROR)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND NAMES speed          example)
list(APPEND SRCS  src/timing.cpp src/example.cpp)
add_subdirectory(gd)

foreach(APP_NAME APP_SRC IN ZIP_LISTS NAMES SRCS)
  message(STATUS "APPNAME=${APP_NAME}  APP_SRC=${APP_SRC}")
  
  add_executable(${APP_NAME} 
    ${APP_SRC})
  
  set_target_properties(${APP_NAME}
    PROPERTIES
      CXX_STANDARD 20
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
  )
  target_link_libraries(${APP_NAME} PUBLIC gd)
  target_compile_features(${APP_NAME} PRIVATE cxx_std_20)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
  
  # Debug version will have the symbols in the executables
  # But allow release to be as small as possible while still having its symbols if needed
  string(APPEND COLLECT_CMD
    "$<IF:$<CONFIG:Release>,"
      "objcopy;--only-keep-debug;$<TARGET_FILE_NAME:${APP_NAME}>;$<TARGET_FILE_NAME:${APP_NAME}>.sym,"
      "echo;collecting symobls skipped"
    ">")
  
  string(APPEND STRIP_CMD
    "$<IF:$<CONFIG:Release>,"
      "objcopy;--strip-debug;$<TARGET_FILE_NAME:${APP_NAME}>;$<TARGET_FILE_NAME:${APP_NAME}>.rel,"
      "echo;stripping symobls skipped"
    ">")
  
  add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARTY_DIR}
    COMMAND "${STRIP_CMD}"
    COMMAND "${COLLECT_CMD}"
    COMMENT "Separate symobls from executable"
    # VERBATIM
    COMMAND_EXPAND_LISTS
  )
endforeach()

