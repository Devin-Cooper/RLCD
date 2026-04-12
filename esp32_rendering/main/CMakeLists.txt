idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES st7305 esp_timer i2c_bsp sensors buttons
             wifi_manager ssh_client ble_hid input_queue app
             nvs_flash lwip onebit sdcard_config
)
