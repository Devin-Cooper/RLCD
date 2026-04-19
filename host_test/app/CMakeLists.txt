set(APP_DIR "${RLCD_COMPONENTS}/app")

add_host_test(
    NAME test_dashboard_feed_data
    SOURCES
        test_dashboard_feed_data.cpp
        "${APP_DIR}/src/dashboard_feed.cpp"
    INCLUDES
        "${APP_DIR}/include"
)
