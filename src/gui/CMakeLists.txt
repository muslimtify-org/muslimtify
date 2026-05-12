# GUI target. Entry: src/muslimtify_gui.c. Component sources added under src/gui/
# as the GUI grows; link muslimtify_core for prayer logic, config, cache, etc.

add_executable(muslimtify-gui ${CMAKE_SOURCE_DIR}/src/muslimtify_gui.c)
muslimtify_set_target_defaults(muslimtify-gui)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(muslimtify-gui PRIVATE -Wno-c2y-extensions)
endif()

# ccompose macros trigger pedantic/missing-braces warnings from empty
# variadic args and nested aggregate initializers. Silence here rather
# than polluting every call site.
if(NOT MSVC)
    target_compile_options(muslimtify-gui PRIVATE
        -Wno-pedantic -Wno-missing-braces)
endif()
get_target_property(_ccompose_inc ccompose INTERFACE_INCLUDE_DIRECTORIES)
if(_ccompose_inc)
    target_include_directories(muslimtify-gui SYSTEM PRIVATE ${_ccompose_inc})
endif()

if(WIN32)
    target_include_directories(muslimtify-gui PRIVATE ${curl_SOURCE_DIR}/include)
    target_link_libraries(muslimtify-gui PRIVATE
        muslimtify_core ccompose ole32 runtimeobject CURL::libcurl)
else()
    target_include_directories(muslimtify-gui PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(muslimtify-gui PRIVATE
        muslimtify_core ccompose ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
endif()

install(TARGETS muslimtify-gui DESTINATION ${CMAKE_INSTALL_BINDIR})
