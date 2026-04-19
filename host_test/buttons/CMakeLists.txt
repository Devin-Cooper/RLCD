set(BTN_DIR "${RLCD_COMPONENTS}/buttons")

add_host_test(
    NAME test_button_state_machine
    SOURCES
        test_button_state_machine.cpp
        "${BTN_DIR}/src/buttons.cpp"
    INCLUDES
        "${BTN_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"   # for driver/gpio.h
)
