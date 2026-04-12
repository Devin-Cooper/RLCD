idf_component_register(
    SRCS
        "src/menu.cpp"
        "src/dashboard.cpp"
        "src/terminal_mode.cpp"
        "src/settings.cpp"
    INCLUDE_DIRS "include"
    REQUIRES st7305 wifi_manager ssh_client ble_hid input_queue
             buttons sensors nvs_flash esp_timer onebit
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
