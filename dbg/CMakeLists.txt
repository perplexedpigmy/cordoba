CMAKE_MINIMUM_REquired(VERSION 3.14 FATAL_ERROR)

add_executable(dbg
               src/dbg.cpp)

set_target_properties(dbg
    PROPERTIES
      CXX_STANDARD 17
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
)
target_compile_options(dbg PRIVATE -g3)
target_include_directories(dbg PRIVATE inc)

include(${gitdb_SOURCE_DIR}/cmake/CPM.cmake)
CPMAddPackage(
  NAME git2
  GITHUB_REPOSITORY libgit2/libgit2
  GIT_TAG v1.3.0
)

CPMAddPackage(
  NAME git2
  GITHUB_REPOSITORY libgit2/libgit2
  GIT_TAG v1.3.0

)
CPMAddPackage(
  NAME expected
  GITHUB_REPOSITORY TartanLlama/expected
  GIT_TAG v1.0.0
)

CPMAddPackage("gh:ericniebler/range-v3#0.11.0")
CPMAddPackage("gh:catchorg/Catch2@2.13.7")


target_include_directories(dbg PRIVATE ${git2_SOURCE_DIR}/include)

target_link_libraries(dbg PRIVATE git2)
target_link_libraries(dbg PRIVATE expected)
target_link_libraries(dbg PRIVATE range-v3)


add_executable(dbg-test test/testPathTraverser.cpp)
set_target_properties(dbg-test
    PROPERTIES
      CXX_STANDARD 17
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
)


target_link_libraries(dbg-test PRIVATE Catch2)
target_include_directories(dbg-test PRIVATE inc)

add_test(
    NAME dbg
    COMMAND $<TARGET_FILE:dbg-test> --success
)
