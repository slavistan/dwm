cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_DISABLE_SOURCE_CHANGES on)
set(CMAKE_DISABLE_IN_SOURCE_BUILD on)

project(dwm
    VERSION 6.2.0
    LANGUAGES C)

find_package(X11 REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)

###
### dwm
###

add_executable(dwm dwm.c drw.c util.c)

target_link_libraries(dwm
    X11::X11
    X11::Xinerama
    X11::Xrender
    X11::Xft
    Fontconfig::Fontconfig
    Freetype::Freetype)

target_compile_definitions(dwm
    PRIVATE XINERAMA
    PRIVATE VERSION="${CMAKE_PROJECT_VERSION}")

install(TARGETS dwm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###
### dwmctl
###

add_executable(dwmctl util.c dwmctl.c)

target_link_libraries(dwmctl
    X11::X11)

install(TARGETS dwmctl DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###
### CMake options
###

option(XSYNCHRONIZE "Use X synchronous mode (debugging)" OFF)
if(XSYNCHRONIZE)
    target_compile_definitions(dwm PUBLIC XSYNCHRONIZE)
    target_compile_definitions(dwmctl PUBLIC XSYNCHRONIZE)
endif()