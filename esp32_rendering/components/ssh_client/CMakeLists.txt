# SSH client component — transitional during the libssh swap.
# After Phase 6 commits, libssh2_esp is removed from PRIV_REQUIRES and
# main/idf_component.yml's dependency. Until then we require BOTH so the
# tree remains buildable while ssh_client.cpp is rewritten in-place.

idf_component_register(
    SRCS
        "src/ssh_client.cpp"
        "src/known_hosts_parser.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls nvs_flash lwip esp_timer esp_system libssh
    PRIV_REQUIRES libssh2_esp
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
