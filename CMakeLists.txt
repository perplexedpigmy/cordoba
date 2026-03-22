cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(cordoba VERSION 0.3.0)

unset(ENV{GIT_DIR})

# Configuration support
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
    STRINGS Release Debug
)

# Build tests/examples OFF by default (consumers don't need them)
option(gd_BUILD_APPS "Build example/test executables" OFF)

# libgit2 configuration - disable problematic features
set(USE_NSEC OFF CACHE BOOL "Support nanosecond precision file mtimes" FORCE)
set(BUILD_CLAR OFF CACHE BOOL "Build clar tests" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "Build fuzzers" FORCE)

# Dependencies - only needed when building apps
if(gd_BUILD_APPS)
  enable_testing()
endif()

# Library
add_subdirectory(gd)

# Examples/apps - only when gd_BUILD_APPS is ON
if(gd_BUILD_APPS)
  add_subdirectory(examples)
endif()

# print available variables
function(dump_cmake_variables)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        if (ARGV0)
            unset(MATCHED)
            string(REGEX MATCH ${ARGV0} MATCHED ${_variableName})
            if (NOT MATCHED)
                continue()
            endif()
        endif()
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
endfunction()
 # dump_cmake_variables()
