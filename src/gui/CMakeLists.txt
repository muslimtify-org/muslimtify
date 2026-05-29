# GUI build. Entry: src/muslimtify_gui.c. Component sources compile into the
# muslimtify_gui_lib OBJECT library; the executable links it plus muslimtify_core.

add_library(muslimtify_gui_lib OBJECT
    ${CMAKE_SOURCE_DIR}/src/gui/app/assets.c
    ${CMAKE_SOURCE_DIR}/src/gui/components/topbar.c
    ${CMAKE_SOURCE_DIR}/src/gui/components/navigation.c
    ${CMAKE_SOURCE_DIR}/src/gui/components/dashboard/prayer_card.c
    ${CMAKE_SOURCE_DIR}/src/gui/components/dashboard/calculation_profile_card.c
    ${CMAKE_SOURCE_DIR}/src/gui/components/dashboard/daily_schedule.c
    ${CMAKE_SOURCE_DIR}/src/gui/components/dashboard/dashboard_content.c
    ${CMAKE_SOURCE_DIR}/src/gui/utils/gui_config.c
    ${CMAKE_SOURCE_DIR}/src/gui/utils/gui_prayer.c
    ${CMAKE_SOURCE_DIR}/src/gui/utils/strfmt.c
    ${CMAKE_SOURCE_DIR}/src/gui/app/app.c)
muslimtify_set_target_defaults(muslimtify_gui_lib)
target_include_directories(muslimtify_gui_lib PUBLIC ${CMAKE_SOURCE_DIR}/src/gui)
# muslimtify_set_target_defaults resolves CMAKE_CURRENT_SOURCE_DIR to src/gui here,
# so the core headers in include/ and src/ are not on the path. Add them explicitly.
target_include_directories(muslimtify_gui_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(muslimtify_gui_lib PRIVATE -Wno-c2y-extensions)
endif()

# ccompose macros trigger pedantic/missing-braces warnings from empty variadic
# args and nested aggregate initializers. Silence here rather than at call sites.
if(NOT MSVC)
    target_compile_options(muslimtify_gui_lib PRIVATE -Wno-pedantic -Wno-missing-braces)
endif()

get_target_property(_ccompose_inc ccompose INTERFACE_INCLUDE_DIRECTORIES)
if(_ccompose_inc)
    target_include_directories(muslimtify_gui_lib SYSTEM PUBLIC ${_ccompose_inc})
endif()

add_executable(muslimtify-gui ${CMAKE_SOURCE_DIR}/src/muslimtify_gui.c)
muslimtify_set_target_defaults(muslimtify-gui)
target_include_directories(muslimtify-gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(muslimtify-gui PRIVATE -Wno-c2y-extensions)
endif()

if(NOT MSVC)
    target_compile_options(muslimtify-gui PRIVATE -Wno-pedantic -Wno-missing-braces)
endif()

if(_ccompose_inc)
    target_include_directories(muslimtify-gui SYSTEM PRIVATE ${_ccompose_inc})
endif()

if(WIN32)
    target_include_directories(muslimtify-gui PRIVATE ${curl_SOURCE_DIR}/include)
    target_link_libraries(muslimtify-gui PRIVATE
        muslimtify_gui_lib muslimtify_core ccompose ole32 runtimeobject CURL::libcurl)
else()
    target_include_directories(muslimtify-gui PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(muslimtify-gui PRIVATE
        muslimtify_gui_lib muslimtify_core ccompose ${LIBNOTIFY_LIBRARIES} ${LIBCURL_LIBRARIES} m)
endif()

install(TARGETS muslimtify-gui DESTINATION ${CMAKE_INSTALL_BINDIR})
