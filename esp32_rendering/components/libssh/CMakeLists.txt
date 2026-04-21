# In-tree ESP-IDF component wrapping libssh 0.11.4, vendored from
# ewpa/LibSSH-ESP32. See README.md for the upstream pin SHA and the
# refresh procedure.

file(GLOB LIBSSH_UPSTREAM_SRC
    "upstream/src/*.c"
    "upstream/external/*.c"
)

# Sources that fail to link under ESP-IDF's mbedTLS-only client build.
# Populated empirically during the first build iteration (Task 1.4 Step 2).
set(LIBSSH_EXCLUDE "")
if(LIBSSH_EXCLUDE)
    list(REMOVE_ITEM LIBSSH_UPSTREAM_SRC ${LIBSSH_EXCLUDE})
endif()

idf_component_register(
    SRCS
        ${LIBSSH_UPSTREAM_SRC}
        "port/libssh_port.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "upstream/src"
        "port"
    REQUIRES
        mbedtls
        pthread
        lwip
        esp_system
        log
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WITH_MBEDTLS=1
    HAVE_ECC=1
    HAVE_DSA=0
    HAVE_LIBMBEDCRYPTO=1
    HAVE_PTHREAD_H=1
    CONFIG_H_INCLUDED=1
    # ewpa's upstream source uses -DESP32 to suppress its own nfds_t typedef
    # when building on ESP-IDF 4+. Without it, libssh/poll.h redefines nfds_t
    # and conflicts with newlib's sys/poll.h (unsigned int).
    ESP32=1
)

# We don't own the vendored C code; keep vendored warnings from breaking the build.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-format
    -Wno-error=char-subscripts
)
