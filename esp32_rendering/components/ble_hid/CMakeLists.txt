idf_component_register(
    SRCS "src/ble_hid.cpp"
         "src/hid_translate.cpp"
         "src/hid_report_process.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_timer
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
