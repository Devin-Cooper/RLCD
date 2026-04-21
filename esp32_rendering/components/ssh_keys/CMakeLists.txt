# Managed SSH key store — NVS index + LittleFS blobs, wraps libssh PKI.
# Screens live under components/app/; this component has no UI code.

idf_component_register(
    SRCS
        "src/key_id.cpp"
        "src/key_store.cpp"
        "src/key_codec.cpp"
        "src/key_gen.cpp"
        "src/export_text.cpp"
        "src/export_qr.cpp"
        "src/export_sd.cpp"
        "src/export_repl.cpp"
        "src/export_enroll.cpp"
        "src/third_party/qrcodegen.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src/third_party"
    REQUIRES
        nvs_flash
        joltwallet__littlefs
        libssh
        mbedtls
        wifi_manager
        ssh_client
        sdcard_config
        esp_system
        log
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# nayuki C file warns on -Wconversion; isolate to keep the rest of the
# component warning-clean.
set_source_files_properties(
    src/third_party/qrcodegen.c
    PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-sign-compare"
)
