idf_component_register(
    SRCS "src/st7305.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd rendering esp_psram
)

# Enable C++17
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
