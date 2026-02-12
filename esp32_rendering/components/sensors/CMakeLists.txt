idf_component_register(
    SRCS "src/pcf85063.cpp" "src/shtc3.cpp" "src/battery.cpp"
    INCLUDE_DIRS "include"
    REQUIRES i2c_bsp esp_adc
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
