CMAKE_MINIMUM_REQUIRED(VERSION 3.14 FATAL_ERROR)

# Examples are built as part of gd_BUILD_APPS option
# This directory should only be included when gd_BUILD_APPS is ON

list(APPEND NAMES speed          example)
list(APPEND SRCS  timing.cpp     example.cpp)

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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  list(APPEND BUILD_PROPERTIES
    CXX_FLAGS "-fcoroutines"
  )
endif()

foreach(APP_NAME APP_SRC IN ZIP_LISTS NAMES SRCS)
  message(STATUS "Building example: ${APP_NAME} from ${APP_SRC}")
  
  add_executable(${APP_NAME} 
    ${APP_SRC})
  
  set_target_properties(${APP_NAME}
    PROPERTIES
      ${BUILD_PROPERTIES}
  )
  
  # Link with gd library (OpenSSL is transitively linked through libgit2)
  target_link_libraries(${APP_NAME} PUBLIC gd::gd)
  
  # Include directories for local headers and library headers
  target_include_directories(${APP_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/gd/inc
  )
  
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
