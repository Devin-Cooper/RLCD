idf_component_register(
    SRCS
        "src/framebuffer.cpp"
        "src/mask_buffer.cpp"
        "src/primitives.cpp"
        "src/patterns.cpp"
        "src/bezier.cpp"
        "src/vector_font.cpp"
        "src/animation.cpp"
        "src/shapes.cpp"
        "src/clock_face.cpp"
        "src/brush_cache.cpp"
        "src/dirty_tracker.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_psram
)

# Enable C++17
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
