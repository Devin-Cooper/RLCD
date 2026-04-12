idf_component_register(
    SRCS "src/wifi_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event nvs_flash esp_netif esp_log
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
