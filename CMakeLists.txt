cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(cordoba VERSION 0.1.0)

unset(ENV{GIT_DIR})

# Configuration support
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
    STRINGS Release Debug
)

# Library builds tests by default (OFF for consumers via CPM)
option(gd_BUILD_APPS "Build example/test executables" ON)

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
