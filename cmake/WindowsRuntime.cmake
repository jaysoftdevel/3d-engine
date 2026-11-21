# All native executables and the engine DLL share one per-configuration directory.
# SDL is built in a dependency subdirectory; stage its DLL beside every executable.
if(WIN32)
 get_property(_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
 foreach(_target IN LISTS _targets)
  get_target_property(_type ${_target} TYPE)
  if(_type STREQUAL "EXECUTABLE")
   add_custom_command(TARGET ${_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL2>" "$<TARGET_FILE_DIR:${_target}>"
    VERBATIM)
  endif()
 endforeach()
endif()
