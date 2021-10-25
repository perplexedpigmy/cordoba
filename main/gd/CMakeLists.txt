CMAKE_MINIMUM_REquired(VERSION 3.14 FATAL_ERROR)

add_library(gd STATIC
            src/gd.cpp)

set_target_properties(gd
    PROPERTIES
      CXX_STANDARD 17
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
      SOVERSION 0.1
      DEBUG_POSTFIX _d
)

# ############################################################
# TODO: Release separate debug information from release

# PUBLIC: Forces
# -frecord-gcc-switches can be read via
#   readelf -p .GCC.command.line <binary>
#   readelf -wi <binary> | grep DW_AT_producer
list(APPEND CXX_FLAGS_DEBUG   -p -g3 -Og -frecord-gcc-switches)
list(APPEND CXX_FLAGS_RELEASE -g3)

target_compile_options(gd PUBLIC
                       $<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>
                       $<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>
                       )

target_include_directories(gd PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
  $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(../../cmake/CPM.cmake)
# CPMAddPackage(
#   NAME git2
#   GITHUB_REPOSITORY libgit2/libgit2
#   GIT_TAG v1.3.0
# )

CPMAddPackage("gh:libgit2/libgit2#v1.3.0")
CPMAddPackage("gh:TartanLlama/expected#v1.0.0")
CPMAddPackage("gh:catchorg/Catch2@2.13.7")

target_include_directories(gd PUBLIC ${libgit2_SOURCE_DIR}/include)

target_link_libraries(gd PRIVATE git2)
target_link_libraries(gd PUBLIC expected)

# export(TARGETS gd FILE MyLibraryConfig.cmake)

add_executable(gdtest test/testPathTraverser.cpp)
target_include_directories(gdtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(gdtest
  PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(gdtest PRIVATE Catch2)
target_include_directories(gdtest PRIVATE inc)

add_test( NAME pathTraverse COMMAND $<TARGET_FILE:gdtest> --success)
