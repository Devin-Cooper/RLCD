if(CONFIG_TEST_CONSOLE_ENABLED)
    set(TEST_CONSOLE_SRCS
        "src/test_console.cpp"
        "src/commands_runtime.cpp"
        "src/commands_introspect.cpp"
    )
else()
    set(TEST_CONSOLE_SRCS "")
endif()

idf_component_register(
    SRCS ${TEST_CONSOLE_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES
        app
        console
        input_queue
        wifi_manager
        ble_hid
        ssh_client
        sdcard_config
        nvs_flash
        esp_system
        log
        espcoredump
        vfs
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
