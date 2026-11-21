# Included only when the engine is configured with JSD_BUILD_SHOWCASE=ON.
add_executable(jsd-showcase src/main.cpp src/showcase.cpp src/cave.cpp src/project_export.cpp)
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
    add_executable(jsd-showcase-physics-tests tests/physics_tests.cpp src/showcase.cpp src/cave.cpp src/project_export.cpp)
    target_link_libraries(jsd-showcase-physics-tests PRIVATE JSD::GameEngine)
    target_compile_definitions(jsd-showcase-physics-tests PRIVATE JSD_SHOWCASE_TEST_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    set_target_properties(jsd-showcase-physics-tests PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS jsd-showcase-physics-tests RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    add_test(NAME showcase-physics COMMAND jsd-showcase-physics-tests)
    add_executable(jsd-rover-tests tests/rover_tests.cpp)
    target_link_libraries(jsd-rover-tests PRIVATE JSD::GameEngine)
    add_test(NAME rover-kinematics COMMAND jsd-rover-tests)
    add_test(NAME showcase-cave-smoke COMMAND jsd-showcase --offscreen --frames 2 --time 92 --width 640 --height 360 --validate)
    add_test(NAME showcase-smoke COMMAND jsd-showcase
        --offscreen --frames 2 --time 40 --width 640 --height 360 --validate)
endif()
