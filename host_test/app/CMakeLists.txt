set(APP_DIR "${RLCD_COMPONENTS}/app")

add_host_test(
    NAME test_animator
    SOURCES
        test_animator.cpp
        "${APP_DIR}/src/animator.cpp"
    INCLUDES
        "${APP_DIR}/include"
)

add_host_test(
    NAME test_dashboard_feed_data
    SOURCES
        test_dashboard_feed_data.cpp
        "${APP_DIR}/src/dashboard_feed.cpp"
    INCLUDES
        "${APP_DIR}/include"
)

add_host_test(
    NAME test_screen_stack
    SOURCES
        test_screen_stack.cpp
        "${APP_DIR}/src/screen_stack.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${RLCD_COMPONENTS}/input_queue/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_breadcrumb
    SOURCES
        test_breadcrumb.cpp
        "${APP_DIR}/src/screen_stack.cpp"
        "${APP_DIR}/src/breadcrumb.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${RLCD_COMPONENTS}/input_queue/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_overlay_manager
    SOURCES
        test_overlay_manager.cpp
        "${APP_DIR}/src/overlay.cpp"
        "${APP_DIR}/src/animator.cpp"
        "${APP_DIR}/src/screen_stack.cpp"
        "${APP_DIR}/src/breadcrumb.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${RLCD_COMPONENTS}/input_queue/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
    DEFINES
        RLCD_HOST_TEST
)

add_host_test(
    NAME test_text_input
    SOURCES
        test_text_input.cpp
        "${APP_DIR}/src/text_input.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_server_creds_store
    SOURCES
        test_server_creds_store.cpp
        "${RLCD_COMPONENTS}/sdcard_config/src/config_store_nvs.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/sdcard_config/include"
        "${RLCD_COMPONENTS}/wifi_manager/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_server_migration
    SOURCES
        test_server_migration.cpp
        "${RLCD_COMPONENTS}/sdcard_config/src/config_store_nvs.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/sdcard_config/include"
        "${RLCD_COMPONENTS}/wifi_manager/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_response_parser
    SOURCES
        test_response_parser.cpp
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_known_hosts_parser
    SOURCES
        test_known_hosts_parser.cpp
        "${RLCD_COMPONENTS}/ssh_client/src/known_hosts_parser.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/ssh_client/include"
)

add_host_test(
    NAME test_ssh_keys_id
    SOURCES
        test_ssh_keys_id.cpp
        "${RLCD_COMPONENTS}/ssh_keys/src/key_id.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/ssh_keys/include"
)

add_host_test(
    NAME test_ssh_keys_index
    SOURCES
        test_ssh_keys_index.cpp
        "${RLCD_COMPONENTS}/ssh_keys/src/key_id.cpp"
        "${RLCD_COMPONENTS}/ssh_keys/src/ssh_keys_index.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/ssh_keys/include"
        "${RLCD_COMPONENTS}/ssh_keys/src"
)

add_host_test(
    NAME test_ssh_keys_codec
    SOURCES
        test_ssh_keys_codec.cpp
        "${RLCD_COMPONENTS}/ssh_keys/src/key_codec_host.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/ssh_keys/include"
)
# Needs OpenSSL for SHA-256 + base64 on host.
find_package(OpenSSL REQUIRED)
target_link_libraries(test_ssh_keys_codec PRIVATE OpenSSL::Crypto)

add_host_test(
    NAME test_ssh_keys_select_qr_scale
    SOURCES
        test_ssh_keys_select_qr_scale.cpp
        "${RLCD_COMPONENTS}/ssh_keys/src/select_qr_scale.cpp"
    INCLUDES
        "${RLCD_COMPONENTS}/ssh_keys/include"
)

add_host_test(
    NAME test_command_palette_filter
    SOURCES
        test_command_palette_filter.cpp
        "${APP_DIR}/src/command_palette_filter.cpp"
    INCLUDES
        "${APP_DIR}/include"
)

add_host_test(
    NAME test_command_registry
    SOURCES
        test_command_registry.cpp
        "${APP_DIR}/src/command_registry.cpp"
    INCLUDES
        "${APP_DIR}/include"
        "${RLCD_COMPONENTS}/sdcard_config/include"
        "${RLCD_COMPONENTS}/wifi_manager/include"
        "${RLCD_COMPONENTS}/input_queue/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
)

add_host_test(
    NAME test_time_service_tz
    SOURCES
        test_time_service_tz.cpp
        "${RLCD_COMPONENTS}/time_service/src/time_service.cpp"
        "${RLCD_COMPONENTS}/time_service/src/tz_catalog.cpp"
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
        "${RLCD_COMPONENTS}/time_service/include"
)

add_host_test(
    NAME test_time_service_writeback
    SOURCES
        test_time_service_writeback.cpp
        "${RLCD_COMPONENTS}/time_service/src/time_service.cpp"
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
        "${RLCD_COMPONENTS}/time_service/include"
)

add_host_test(
    NAME test_ssh_keys_qr_encode
    SOURCES
        test_ssh_keys_qr_encode.cpp
        "${RLCD_COMPONENTS}/ssh_keys/src/third_party/qrcodegen.c"
    INCLUDES
        "${RLCD_COMPONENTS}/ssh_keys/src/third_party"
    DEFINES
        FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
