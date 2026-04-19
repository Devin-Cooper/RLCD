idf_component_register(
    SRCS
        "src/framebuffer.cpp"
        "src/primitives.cpp"
        "src/dirty_tracker.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_psram
)

# Enable C++17
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
