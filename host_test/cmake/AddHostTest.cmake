# add_host_test(NAME <test_name> SOURCES <files...> [INCLUDES <dirs...>] [DEFINES <defs...>])
#
# Creates an executable that links Catch2WithMain and registers every
# TEST_CASE with ctest via catch_discover_tests().
function(add_host_test)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES INCLUDES DEFINES)
    cmake_parse_arguments(HT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT HT_NAME)
        message(FATAL_ERROR "add_host_test: NAME is required")
    endif()
    if(NOT HT_SOURCES)
        message(FATAL_ERROR "add_host_test: SOURCES is required")
    endif()

    add_executable(${HT_NAME} ${HT_SOURCES})
    target_link_libraries(${HT_NAME} PRIVATE Catch2::Catch2WithMain)
    target_include_directories(${HT_NAME} PRIVATE ${RLCD_SHIMS} ${HT_INCLUDES})
    target_compile_definitions(${HT_NAME} PRIVATE RLCD_HOST_TEST)
    if(HT_DEFINES)
        target_compile_definitions(${HT_NAME} PRIVATE ${HT_DEFINES})
    endif()
    catch_discover_tests(${HT_NAME})
endfunction()
