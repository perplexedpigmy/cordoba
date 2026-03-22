CMAKE_MINIMUM_REQUIRED(VERSION 3.14 FATAL_ERROR)

# Support BUILD_SHARED_LIBS - CMake standard
# Default to STATIC if not specified
if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS OFF)
endif()

add_library(gd STATIC)
add_library(gd::gd ALIAS gd)

# Add sources
target_sources(gd
  PRIVATE
    src/guard.cpp
    src/gd.cpp
)

set_target_properties(gd
  PROPERTIES
    CXX_STANDARD 23
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    SOVERSION 0
    VERSION @gd_VERSION@
    DEBUG_POSTFIX _d
)

# Debug/Release compile flags
list(APPEND CXX_FLAGS_DEBUG   -g3 -frecord-gcc-switches)
list(APPEND CXX_FLAGS_RELEASE -g3)

target_compile_options(gd PUBLIC
  $<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>
  $<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>
)

target_include_directories(gd PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
  $<INSTALL_INTERFACE:include/gd>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Dependencies
include(../cmake/CPM.cmake)

# Avoid warnings in libgit2 build
include(CheckCCompilerFlag)
check_c_compiler_flag(-Wno-incompatible-pointer-types SUPPRESS_INCOMPATIBLE_PT_WARNING)
check_c_compiler_flag(-Wno-misleading-indentation SUPPRESS_MISLEADING_INDENTATION)

# Fetch libgit2 v1.4.2 (last version with 'git2' static library target)
# v1.5+ switched to OBJECT library which has linking issues
CPMAddPackage("gh:libgit2/libgit2@1.4.2")

# Link against libgit2
# v1.4.x uses 'git2' target
if(TARGET git2)
  message(STATUS "Linking against git2 (libgit2 v1.4.x)")
  target_compile_options(git2 PRIVATE -DUSE_THREADS=ON)
  if(SUPPRESS_INCOMPATIBLE_PT_WARNING)
    target_compile_options(git2 PRIVATE -Wno-incompatible-pointer-types)
  endif()
  if(SUPPRESS_MISLEADING_INDENTATION)
    target_compile_options(git2 PRIVATE -Wno-misleading-indentation)
  endif()
  target_link_libraries(gd PRIVATE git2)
elseif(TARGET libgit2)
  # v1.5+ uses 'libgit2' OBJECT library
  message(STATUS "Linking against libgit2 OBJECT library")
  target_link_libraries(gd PRIVATE libgit2)
else()
  message(FATAL_ERROR "Could not find libgit2 target")
endif()

target_include_directories(gd PUBLIC ${libgit2_SOURCE_DIR}/include)

CPMAddPackage("gh:catchorg/Catch2@3.7.0")
CPMAddPackage("gh:gabime/spdlog@1.15.0")
CPMAddPackage("gh:CLIUtils/CLI11@2.4.2")

target_link_libraries(gd PUBLIC spdlog)

# Sanitizer support
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

# Tests - only when BUILD_APPS is ON
if(gd_BUILD_APPS)
  # crud test (Catch2 unit tests)
  add_executable(crud test/crud.cpp)
  target_include_directories(crud PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  set_target_properties(crud
    PROPERTIES
      CXX_STANDARD 23
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
  )
  target_link_libraries(crud PUBLIC CLI11)
  target_link_libraries(crud PRIVATE Catch2WithMain gd::gd stdc++fs)
  target_include_directories(crud PRIVATE inc)
  add_test(NAME crud COMMAND $<TARGET_FILE:crud>)

  # greens test (stress test with arguments)
  add_executable(greens test/greens.cpp)
  target_include_directories(greens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  set_target_properties(greens
    PROPERTIES
      CXX_STANDARD 23
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
  )
  target_link_libraries(greens PUBLIC CLI11)
  target_link_libraries(greens PRIVATE Catch2WithMain gd::gd stdc++fs)
  target_include_directories(greens PRIVATE inc)
  add_test(NAME greens COMMAND $<TARGET_FILE:greens> -g 5 -b 10 -c 30 -o 50)
endif()

# Note: For CPM-based consumption, the library is built as part of the consumer's project.
# No traditional install/export is needed. The library target (gd::gd) is available
# automatically via target_link_libraries().
