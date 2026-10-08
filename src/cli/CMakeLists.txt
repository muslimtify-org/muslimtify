# The command line presentation layer: the dispatcher, one file per command,
# their headers, and the entry point of the muslimtify binary. It links the
# back end through muslimtify_core and includes only muslimtify.h and the
# platform interface headers from include/.

add_library(muslimtify_cli OBJECT
    cli.c
    display.c
    cmd_show.c
    cmd_location.c
    cmd_prayer.c
    $<IF:$<BOOL:${WIN32}>,cmd_daemon_win.c,cmd_daemon.c>
    cmd_method.c
    cmd_notification.c
    cmd_timeformat.c
    # daemon_loop.c is POSIX-only (sigaction/nanosleep); the Windows scheduled
    # task runs muslimtify-service.exe and never calls run_daemon_loop.
    $<$<NOT:$<BOOL:${WIN32}>>:daemon_loop.c>
)
muslimtify_set_target_defaults(muslimtify_cli)
# The CLI's own headers, for the binary and for test_cli, which link this target.
target_include_directories(muslimtify_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(muslimtify main.c)
muslimtify_set_target_defaults(muslimtify)

if(WIN32)
    target_include_directories(muslimtify_cli PRIVATE ${curl_SOURCE_DIR}/include)
    target_include_directories(muslimtify PRIVATE ${curl_SOURCE_DIR}/include)
    # Static curl: prevent its headers from emitting __declspec(dllimport).
    target_compile_definitions(muslimtify_cli PRIVATE CURL_STATICLIB)
    target_compile_definitions(muslimtify PRIVATE CURL_STATICLIB)
    target_link_libraries(muslimtify
        muslimtify_cli muslimtify_core ole32 runtimeobject CURL::libcurl)
else()
    target_include_directories(muslimtify_cli PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_include_directories(muslimtify PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(muslimtify
        muslimtify_cli muslimtify_core ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
endif()

install(TARGETS muslimtify DESTINATION ${CMAKE_INSTALL_BINDIR})
