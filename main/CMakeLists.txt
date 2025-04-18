CMAKE_MINIMUM_REquired(VERSION 3.14 FATAL_ERROR)

list(APPEND NAMES speed          example)
list(APPEND SRCS  src/timing.cpp src/example.cpp)

add_subdirectory(gd)

set(BUILD_PROPERTIES
  CXX_STANDARD 23
  CXX_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED ON
)

if(SANITIZE)
  list(APPEND BUILD_PROPERTIES
    CXX_FLAGS "-fsanitize=address"
    LINK_FLAGS "-fsanitize=address"
  )
endif()

foreach(APP_NAME APP_SRC IN ZIP_LISTS NAMES SRCS)
  message(STATUS "APPNAME=${APP_NAME}  APP_SRC=${APP_SRC}")
  
  add_executable(${APP_NAME} 
    ${APP_SRC})
  
  set_target_properties(${APP_NAME}
    PROPERTIES
      ${BUILD_PROPERTIES}
  )
  target_link_libraries(${APP_NAME} PUBLIC gd)
  target_compile_features(${APP_NAME} PRIVATE cxx_std_20)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
  
  # Debug version will have the symbols in the executables
  # But allow release to be as small as possible while still having its symbols if needed

  string(APPEND COLLECT_${APP_NAME}_CMD
    "$<IF:$<CONFIG:Release>,"
      "objcopy;--only-keep-debug;$<TARGET_FILE:${APP_NAME}>;$<TARGET_FILE:${APP_NAME}>.sym,"
      "echo;collecting symbols skipped"
    ">")
  
  string(APPEND STRIP_${APP_NAME}_CMD
    "$<IF:$<CONFIG:Release>,"
      "objcopy;--strip-debug;$<TARGET_FILE:${APP_NAME}>;$<TARGET_FILE:${APP_NAME}>.rel,"
      "echo;stripping symbols skipped"
    ">")
  
  add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND "${STRIP_${APP_NAME}_CMD}"
    COMMAND "${COLLECT_CMD}"
    COMMENT "Separate symbols from executable"
    # VERBATIM
    COMMAND_EXPAND_LISTS
  )
endforeach()

