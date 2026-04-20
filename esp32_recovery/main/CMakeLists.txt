idf_component_register(
    SRCS
        "recovery_main.cpp"
        "recovery_screen.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_system
        esp_timer
        esp_app_format
        log
        freertos
        driver
        esp_lcd
        esp_psram
        rendering
        st7305
        onebit
)
