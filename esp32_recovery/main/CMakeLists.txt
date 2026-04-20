idf_component_register(
    SRCS
        "recovery_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_system
        esp_timer
        log
        freertos
)
