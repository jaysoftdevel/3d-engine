# Included only when the engine is configured with JSD_BUILD_SHOWCASE=ON.
add_executable(jsd-showcase src/main.cpp src/showcase.cpp)
target_link_libraries(jsd-showcase PRIVATE JSD::GameEngine)
target_compile_definitions(jsd-showcase PRIVATE
    JSD_SHOWCASE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    JSD_SHOWCASE_ASSET_SUBDIR="${CMAKE_INSTALL_DATADIR}/jsd-game-engine/showcase/assets")
set_target_properties(jsd-showcase PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
install(TARGETS jsd-showcase RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/jsd-game-engine/showcase/assets)
install(DIRECTORY docs licenses DESTINATION ${CMAKE_INSTALL_DATADIR}/jsd-game-engine/showcase)
install(FILES README.md LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/jsd-game-engine/showcase)
if(JSD_BUILD_TESTS)
    add_test(NAME showcase-smoke COMMAND jsd-showcase
        --offscreen --frames 2 --time 40 --width 640 --height 360 --validate)
endif()
