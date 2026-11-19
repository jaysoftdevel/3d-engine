#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "JSD::GameEngine" for configuration "Release"
set_property(TARGET JSD::GameEngine APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(JSD::GameEngine PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libjsd-game-engine.so.0.4.0"
  IMPORTED_SONAME_RELEASE "libjsd-game-engine.so.0"
  )

list(APPEND _cmake_import_check_targets JSD::GameEngine )
list(APPEND _cmake_import_check_files_for_JSD::GameEngine "${_IMPORT_PREFIX}/lib/libjsd-game-engine.so.0.4.0" )

# Import target "JSD::GameEngineStatic" for configuration "Release"
set_property(TARGET JSD::GameEngineStatic APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(JSD::GameEngineStatic PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libjsd-game-engine.a"
  )

list(APPEND _cmake_import_check_targets JSD::GameEngineStatic )
list(APPEND _cmake_import_check_files_for_JSD::GameEngineStatic "${_IMPORT_PREFIX}/lib/libjsd-game-engine.a" )

# Import target "JSD::Jolt" for configuration "Release"
set_property(TARGET JSD::Jolt APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(JSD::Jolt PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libJolt.a"
  )

list(APPEND _cmake_import_check_targets JSD::Jolt )
list(APPEND _cmake_import_check_files_for_JSD::Jolt "${_IMPORT_PREFIX}/lib/libJolt.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
