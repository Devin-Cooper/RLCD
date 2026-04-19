set(BLE_HID_DIR "${RLCD_COMPONENTS}/ble_hid")

add_host_test(
    NAME test_translate_keycode
    SOURCES
        test_translate_keycode.cpp
        "${BLE_HID_DIR}/src/hid_translate.cpp"
    INCLUDES
        "${BLE_HID_DIR}/include"
)
