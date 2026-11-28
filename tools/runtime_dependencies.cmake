cmake_minimum_required(VERSION 3.21)

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${executables}
    LIBRARIES ${shared}
    DIRECTORIES ${search_dirs}
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    CONFLICTING_DEPENDENCIES_PREFIX conflicting)

foreach(path IN LISTS resolved)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "file|${path}")
endforeach()
foreach(name IN LISTS unresolved)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "unresolved at build time|${name}")
endforeach()
foreach(name IN LISTS conflicting_FILENAMES)
    foreach(path IN LISTS conflicting_${name})
        execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "file|${path}")
    endforeach()
endforeach()
