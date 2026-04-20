idf_component_register(
    SRCS
        "src/menu.cpp"
        "src/dashboard.cpp"
        "src/dashboard_feed.cpp"
        "src/terminal_mode.cpp"
        "src/settings.cpp"
        "src/screen_stack.cpp"
    INCLUDE_DIRS "include"
    REQUIRES onebit ssh_client sdcard_config nvs_flash input_queue
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
