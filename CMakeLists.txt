cmake_minimum_required(VERSION 3.22...3.31)
project(muslimtify VERSION 0.2.2 LANGUAGES C)

# -- Default build type --------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(FULL_PROJECT_VERSION "${PROJECT_VERSION}")

# -- C standard and output dirs ------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(GNUInstallDirs)
include(FetchContent)

# -- Dependencies --------------------------------------------------------------

if(WIN32)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(curl
        URL https://github.com/curl/curl/releases/download/curl-8_11_1/curl-8.11.1.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
    pkg_check_modules(LIBCURL REQUIRED libcurl)
endif()

# -- Generated version header -------------------------------------------------

configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

# -- Helper function ----------------------------------------------------------

function(muslimtify_set_target_defaults target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
        )
        if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
            target_compile_options(${target} PRIVATE -Wno-c11-extensions)
        endif()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

# -- Object libraries ---------------------------------------------------------
# muslimtify_core: platform-agnostic + platform abstraction
# muslimtify_cli : CLI dispatcher + command handlers

add_library(muslimtify_core OBJECT
    src/core/config.c
    src/core/cache.c
    src/core/location.c
    src/core/string_util.c
    src/core/country.c
    src/core/prayer_checker.c
    src/core/check_cycle.c
    src/core/display.c
    $<IF:$<BOOL:${WIN32}>,src/platform/windows/notification_win.c,src/platform/linux/notification.c>
    $<IF:$<BOOL:${WIN32}>,src/platform/windows/platform_win.c,src/platform/linux/platform_linux.c>
    $<IF:$<BOOL:${WIN32}>,src/platform/windows/timezone.c,src/platform/linux/timezone.c>
)
muslimtify_set_target_defaults(muslimtify_core)

add_library(muslimtify_cli OBJECT
    src/cli/cli.c
    src/cli/cmd_show.c
    src/cli/cmd_next.c
    src/cli/cmd_config.c
    src/cli/cmd_location.c
    src/cli/cmd_prayer.c
    $<IF:$<BOOL:${WIN32}>,src/cli/cmd_daemon_win.c,src/cli/cmd_daemon.c>
    src/cli/cmd_method.c
    src/cli/cmd_notification.c
    src/cli/cmd_sound.c
)
muslimtify_set_target_defaults(muslimtify_cli)

if(WIN32)
    target_include_directories(muslimtify_core PRIVATE ${curl_SOURCE_DIR}/include)
    target_include_directories(muslimtify_cli  PRIVATE ${curl_SOURCE_DIR}/include)
    # Static curl: prevent its headers from emitting __declspec(dllimport).
    target_compile_definitions(muslimtify_core PRIVATE CURL_STATICLIB)
    target_compile_definitions(muslimtify_cli  PRIVATE CURL_STATICLIB)
else()
    target_include_directories(muslimtify_core PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_include_directories(muslimtify_cli  PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
endif()

# -- Main executable ----------------------------------------------------------

add_executable(muslimtify src/muslimtify.c)
muslimtify_set_target_defaults(muslimtify)

if(WIN32)
    target_include_directories(muslimtify PRIVATE ${curl_SOURCE_DIR}/include)
    target_compile_definitions(muslimtify PRIVATE CURL_STATICLIB)
    target_link_libraries(muslimtify
        muslimtify_cli muslimtify_core ole32 runtimeobject CURL::libcurl)
else()
    target_include_directories(muslimtify PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(muslimtify
        muslimtify_cli muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
endif()

# -- Testing ------------------------------------------------------------------

option(BUILD_TESTING "Build test targets" ON)

if(BUILD_TESTING)
    enable_testing()

    if(NOT WIN32)
        add_executable(test_cli tests/test_cli.c)
        muslimtify_set_target_defaults(test_cli)
        target_include_directories(test_cli PRIVATE ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(test_cli
            muslimtify_cli muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m
        )
        add_test(NAME cli COMMAND test_cli)

        add_executable(test_prayer_checker tests/test_prayer_checker.c)
        muslimtify_set_target_defaults(test_prayer_checker)
        target_include_directories(test_prayer_checker PRIVATE ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(test_prayer_checker muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
        add_test(NAME prayer_checker COMMAND test_prayer_checker)

        add_executable(test_config tests/test_config.c)
        muslimtify_set_target_defaults(test_config)
        target_include_directories(test_config PRIVATE ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(test_config muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
        add_test(NAME config COMMAND test_config)

        add_executable(test_cache tests/test_cache.c)
        muslimtify_set_target_defaults(test_cache)
        target_include_directories(test_cache PRIVATE ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(test_cache muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
        add_test(NAME cache COMMAND test_cache)
    endif()

    # test_location is cross-platform; its parse_timezone_offset target lives
    # in muslimtify_core which links curl on both platforms.
    add_executable(test_location tests/test_location.c)
    muslimtify_set_target_defaults(test_location)
    if(WIN32)
        target_include_directories(test_location PRIVATE ${curl_SOURCE_DIR}/include)
        target_link_libraries(test_location muslimtify_core CURL::libcurl ole32 runtimeobject)
    else()
        target_include_directories(test_location PRIVATE ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(test_location muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
    endif()
    add_test(NAME location COMMAND test_location)

    # Cross-platform tests (no POSIX dependencies)
    add_executable(test_platform
        tests/test_platform.c
        $<$<BOOL:${WIN32}>:src/platform/windows/platform_win.c>
        $<$<NOT:$<BOOL:${WIN32}>>:src/platform/linux/platform_linux.c>
    )
    muslimtify_set_target_defaults(test_platform)
    add_test(NAME platform COMMAND test_platform)

    add_executable(test_prayertimes tests/test_prayertimes.c)
    muslimtify_set_target_defaults(test_prayertimes)
    if(NOT WIN32)
        target_link_libraries(test_prayertimes m)
    endif()
    add_test(NAME prayertimes COMMAND test_prayertimes)

    add_executable(test_json tests/test_json.c)
    muslimtify_set_target_defaults(test_json)
    if(NOT WIN32)
        target_link_libraries(test_json m)
    endif()
    add_test(NAME json COMMAND test_json)

    add_executable(test_string_util tests/test_string_util.c src/core/string_util.c)
    muslimtify_set_target_defaults(test_string_util)
    add_test(NAME string_util COMMAND test_string_util)

    add_executable(test_country tests/test_country.c src/core/country.c)
    muslimtify_set_target_defaults(test_country)
    add_test(NAME country COMMAND test_country)

    add_executable(test_method_detect tests/test_method_detect.c)
    muslimtify_set_target_defaults(test_method_detect)
    if(NOT WIN32)
        target_link_libraries(test_method_detect m)
    endif()
    add_test(NAME method_detect COMMAND test_method_detect)

    if(WIN32)
        add_executable(test_cmd_daemon_win
            tests/test_cmd_daemon_win.c
            src/cli/cmd_daemon_win.c
            src/platform/windows/platform_win.c
        )
        muslimtify_set_target_defaults(test_cmd_daemon_win)
        target_compile_definitions(test_cmd_daemon_win PRIVATE MUSLIMTIFY_CMD_DAEMON_WIN_TEST)
        add_test(NAME cmd_daemon_win COMMAND test_cmd_daemon_win)

        add_executable(test_notification_win
            tests/test_notification_win.c
            src/platform/windows/notification_win.c
        )
        muslimtify_set_target_defaults(test_notification_win)
        target_compile_definitions(test_notification_win PRIVATE MUSLIMTIFY_NOTIFICATION_WIN_TEST)
        target_link_libraries(test_notification_win ole32 runtimeobject)
        add_test(NAME notification_win COMMAND test_notification_win)
    endif()
endif()

# -- Install ------------------------------------------------------------------

install(TARGETS muslimtify DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES assets/muslimtify.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps/)
install(FILES assets/muslimtify.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps/)
if(WIN32)
    install(FILES assets/muslimtify.ico
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/)
endif()

install(FILES systemd/muslimtify.service systemd/muslimtify.timer
    DESTINATION lib/systemd/user/)

if(WIN32)
    add_executable(muslimtify-service WIN32 src/platform/windows/muslimtify_service_win.c)
    muslimtify_set_target_defaults(muslimtify-service)
    target_link_libraries(muslimtify-service
        muslimtify_core ole32 runtimeobject CURL::libcurl)
    install(TARGETS muslimtify-service DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            RESOLVED_DEPENDENCIES_VAR muslimtify_resolved_deps
            UNRESOLVED_DEPENDENCIES_VAR muslimtify_unresolved_deps
            EXECUTABLES
                "$<TARGET_FILE:muslimtify>"
                "$<TARGET_FILE:muslimtify-service>"
            PRE_EXCLUDE_REGEXES
                "api-ms-win-.*"
                "AzureAttestManager\\.dll"
                "AzureAttestNormal\\.dll"
                "HvsiFileTrust\\.dll"
                "PdmUtilities\\.dll"
                "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[\\\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*"
                ".*[\\\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\\\/][Ss][Yy][Ss][Ww][Oo][Ww]64[\\\\/].*"
        )

        if(muslimtify_unresolved_deps)
            message(STATUS "Ignoring unresolved runtime dependencies: ${muslimtify_unresolved_deps}")
        endif()

        foreach(muslimtify_dep IN LISTS muslimtify_resolved_deps)
            file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                TYPE SHARED_LIBRARY
                FILES "${muslimtify_dep}"
                FOLLOW_SYMLINK_CHAIN)
        endforeach()
    ]])
endif()
