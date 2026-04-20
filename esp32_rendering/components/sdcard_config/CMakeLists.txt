idf_component_register(
    SRCS "src/sdcard_manager.cpp" "src/config_manager.cpp" "src/config_store_nvs.cpp"
    INCLUDE_DIRS "include"
    REQUIRES sdmmc nvs_flash vfs fatfs esp_timer esp_driver_sdmmc esp_driver_gpio wifi_manager
    PRIV_REQUIRES json
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
