cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(gitdb)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


enable_testing()
add_subdirectory(dbg)

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
