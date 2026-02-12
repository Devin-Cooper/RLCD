idf_component_register(
    SRCS "src/buttons.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
