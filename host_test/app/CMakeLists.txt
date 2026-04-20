set(APP_DIR "${RLCD_COMPONENTS}/app")

add_host_test(
    NAME test_dashboard_feed_data
    SOURCES
        test_dashboard_feed_data.cpp
        "${APP_DIR}/src/dashboard_feed.cpp"
    INCLUDES
        "${APP_DIR}/include"
)

add_host_test(
    NAME test_menu_navigation
    SOURCES
        test_menu_navigation.cpp
        "${APP_DIR}/src/menu.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_screen_stack
    SOURCES
        test_screen_stack.cpp
        "${APP_DIR}/src/screen_stack.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${RLCD_COMPONENTS}/input_queue/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_overlay_manager
    SOURCES
        test_overlay_manager.cpp
        "${APP_DIR}/src/overlay.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${RLCD_COMPONENTS}/input_queue/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_text_input
    SOURCES
        test_text_input.cpp
        "${APP_DIR}/src/text_input.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)
