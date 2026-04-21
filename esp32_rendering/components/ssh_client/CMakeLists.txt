# SSH client component — uses in-tree components/libssh/.

idf_component_register(
    SRCS
        "src/ssh_client.cpp"
        "src/known_hosts_parser.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls nvs_flash lwip esp_timer esp_system libssh
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
