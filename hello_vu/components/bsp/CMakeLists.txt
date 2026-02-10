idf_component_register(
    SRCS
        "src/display_bsp.cpp"
        "src/i2c_bsp.cpp"
        "src/audio_bsp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd
)
