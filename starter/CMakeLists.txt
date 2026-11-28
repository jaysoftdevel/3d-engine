cmake_minimum_required(VERSION 3.21)
project(JSDStarter LANGUAGES C CXX)

set(JSD_ENGINE_SOURCE "" CACHE PATH "Optional JSD engine source directory")
if(JSD_ENGINE_SOURCE)
    add_subdirectory("${JSD_ENGINE_SOURCE}" external-jsd EXCLUDE_FROM_ALL)
else()
    find_package(JSDGameEngine 0.5.28 REQUIRED)
endif()

add_executable(starter main.cpp)
target_link_libraries(starter PRIVATE JSD::GameEngine)
if(WIN32)
    add_custom_command(TARGET starter POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:starter> $<TARGET_FILE_DIR:starter>
        COMMAND_EXPAND_LISTS)
endif()
