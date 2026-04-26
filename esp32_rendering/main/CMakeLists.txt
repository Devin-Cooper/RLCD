idf_component_register(
    SRCS "main.cpp" "boot_validator.cpp"
    INCLUDE_DIRS "."
    REQUIRES st7305 esp_timer i2c_bsp sensors buttons
             wifi_manager ssh_client ble_hid input_queue app
             nvs_flash lwip onebit sdcard_config test_console
             app_update time_service
)
