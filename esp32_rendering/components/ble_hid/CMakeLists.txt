idf_component_register(
    SRCS "src/ble_hid.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_timer
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
