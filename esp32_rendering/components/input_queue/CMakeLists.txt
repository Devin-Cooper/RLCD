idf_component_register(
    SRCS "src/input_queue.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_log
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
