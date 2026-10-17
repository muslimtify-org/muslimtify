# The graphical presentation layer: the window, its widgets and its own
# preference file. It links the back end through muslimtify_core and includes
# only muslimtify.h, json.h and the platform interface headers from include/.

set(CCOMPOSE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CCOMPOSE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CCOMPOSE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ccompose
    GIT_REPOSITORY https://github.com/rizukirr/ccompose.git
    GIT_TAG v0.1.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(ccompose)

add_library(muslimtify_gui_lib OBJECT
    themes/fonts.c
    themes/themes.c
    widgets/header.c
    dashboard/components/prayer_time_card.c
    widgets/toggle.c
    dashboard/content.c
    core/gui_config.c
)
muslimtify_set_target_defaults(muslimtify_gui_lib)
# The GUI's own headers, for the binary and for anything that links this target.
target_include_directories(muslimtify_gui_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

get_target_property(_ccompose_inc ccompose INTERFACE_INCLUDE_DIRECTORIES)
if(_ccompose_inc)
    target_include_directories(muslimtify_gui_lib SYSTEM PUBLIC
        ${_ccompose_inc})
endif()

add_executable(muslimtify-gui main.c)
muslimtify_set_target_defaults(muslimtify-gui)

# ccompose's declarative macros rely on compound literals and designated
# initialisers in ways the strict warning set rejects.
foreach(target muslimtify_gui_lib muslimtify-gui)
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PRIVATE -Wno-c2y-extensions)
    endif()
    if(NOT MSVC)
        target_compile_options(
            ${target} PRIVATE -Wno-pedantic -Wno-missing-braces)
    endif()
endforeach()

if(WIN32)
    target_include_directories(muslimtify-gui PRIVATE
        ${curl_SOURCE_DIR}/include)
    target_link_libraries(muslimtify-gui PRIVATE
        muslimtify_gui_lib
        muslimtify_core
        ccompose
        ole32
        runtimeobject CURL::libcurl)
else()
    target_include_directories(muslimtify-gui PRIVATE
        ${LIBNOTIFY_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(muslimtify-gui PRIVATE
        muslimtify_gui_lib
        muslimtify_core
        ccompose
        ${LIBNOTIFY_LIBRARIES}
        ${LIBCURL_LIBRARIES} m)
endif()

install(TARGETS muslimtify-gui DESTINATION ${CMAKE_INSTALL_BINDIR})

# fonts.c looks these up under <prefix>/share/muslimtify/fonts.
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/fonts/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/muslimtify/fonts
    FILES_MATCHING PATTERN "*.ttf")
