idf_component_register(
    SRCS
        "src/menu.cpp"
        "src/dashboard.cpp"
        "src/dashboard_feed.cpp"
        "src/terminal_mode.cpp"
        "src/settings.cpp"
        "src/screen_stack.cpp"
        "src/overlay.cpp"
        "src/text_input.cpp"
        "src/screens/dashboard_screen.cpp"
        "src/screens/terminal_screen.cpp"
        "src/screens/menu_screen.cpp"
        "src/screens/pairing_screen.cpp"
        "src/screens/about_screen.cpp"
        "src/screens/wifi_screen.cpp"
        "src/screens/password_screen.cpp"
        "src/screens/server_list_screen.cpp"
        "src/screens/server_edit_screen.cpp"
        "src/screens/settings_screen.cpp"
    INCLUDE_DIRS "include"
    REQUIRES onebit ssh_client sdcard_config nvs_flash input_queue
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
