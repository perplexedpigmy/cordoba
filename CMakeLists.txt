cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(gitdb)

# Configuration support
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
  STRINGS Release Debug
)

enable_testing()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


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
 dump_cmake_variables()


add_subdirectory(main)
