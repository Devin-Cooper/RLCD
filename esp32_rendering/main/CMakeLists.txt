idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES rendering st7305 esp_timer i2c_bsp sensors buttons
)
