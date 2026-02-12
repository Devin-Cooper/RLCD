idf_component_register(
    SRCS "src/i2c_bsp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
