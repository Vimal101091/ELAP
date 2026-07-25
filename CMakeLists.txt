cmake_minimum_required(VERSION 3.16)

project(ELAP
    VERSION 0.1.0
    DESCRIPTION "Embedded Linux Application Platform"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ELAP_BUILD_TESTS "Build ELAP unit tests" ON)

find_package(Threads REQUIRED)

add_library(elap_platform
    platform/src/config/KeyValueConfiguration.cpp
    platform/src/ipc/UnixSocket.cpp
    platform/src/logging/ConsoleLogger.cpp
    platform/src/service/ServiceApplication.cpp
    platform/src/signals/SignalHandler.cpp
    platform/src/threading/ThreadManager.cpp)

target_include_directories(elap_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/include)

target_link_libraries(elap_platform
    PUBLIC
        Threads::Threads)

target_compile_options(elap_platform
    PRIVATE
        -Wall -Wextra -Wpedantic)

add_executable(sample_service
    applications/sample_service/main.cpp)

target_link_libraries(sample_service
    PRIVATE
        elap_platform)

add_executable(ipc_echo_server
    applications/ipc_echo_server/main.cpp)

target_link_libraries(ipc_echo_server
    PRIVATE
        elap_platform)

add_executable(ipc_echo_client
    applications/ipc_echo_client/main.cpp)

target_link_libraries(ipc_echo_client
    PRIVATE
        elap_platform)

if(ELAP_BUILD_TESTS)
    enable_testing()

    add_executable(elap_unit_tests
        tests/unit/config_tests.cpp
        tests/unit/ipc_tests.cpp
        tests/unit/logger_tests.cpp
        tests/unit/thread_manager_tests.cpp)

    target_link_libraries(elap_unit_tests
        PRIVATE
            elap_platform)

    add_test(NAME elap_unit_tests COMMAND $<TARGET_FILE:elap_unit_tests>)

    add_test(
        NAME sample_service_graceful_shutdown
        COMMAND bash
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/sample_service_shutdown_test.sh
            $<TARGET_FILE:sample_service>)

    add_test(
        NAME sample_service_invalid_config
        COMMAND bash
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/sample_service_invalid_config_test.sh
            $<TARGET_FILE:sample_service>)

    add_test(
        NAME ipc_echo_process_round_trip
        COMMAND bash
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/ipc_echo_process_test.sh
            $<TARGET_FILE:ipc_echo_server>
            $<TARGET_FILE:ipc_echo_client>)
endif()
