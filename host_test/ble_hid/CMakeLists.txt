set(BLE_HID_DIR "${RLCD_COMPONENTS}/ble_hid")

add_host_test(
    NAME test_translate_keycode
    SOURCES
        test_translate_keycode.cpp
        "${BLE_HID_DIR}/src/hid_translate.cpp"
    INCLUDES
        "${BLE_HID_DIR}/include"
)

add_host_test(
    NAME test_process_hid_report
    SOURCES
        test_process_hid_report.cpp
        "${BLE_HID_DIR}/src/hid_translate.cpp"
        "${BLE_HID_DIR}/src/hid_report_process.cpp"
    INCLUDES
        "${BLE_HID_DIR}/include"
)
