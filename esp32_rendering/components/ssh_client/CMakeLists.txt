# SSH client component using skuodi/libssh2_esp
# libssh2 v1.11.1 ESP-IDF component with mbedTLS backend
#
# Install: idf.py add-dependency "skuodi/libssh2_esp^1.11.1"
# Or add to idf_component.yml

idf_component_register(
    SRCS "src/ssh_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls esp_log nvs_flash lwip esp_timer esp_system
    PRIV_REQUIRES libssh2_esp
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
