# Transport shim for the ST7306-class reflective panel. All the panel logic
# lives in the onebit library (components come from ONEBIT_LIB_DIR via
# EXTRA_COMPONENT_DIRS); this component only carries bytes over esp_lcd.
#
# `onebit` must be REQUIRES, not PRIV_REQUIRES: the public header includes
# <1bit/hal/st7306_display.hpp>.
idf_component_register(
    SRCS "src/st7306_panel.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd onebit
)

# Enable C++17
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
