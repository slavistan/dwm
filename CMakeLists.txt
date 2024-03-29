cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_DISABLE_SOURCE_CHANGES on)
set(CMAKE_DISABLE_IN_SOURCE_BUILD on)

project(dwm VERSION 6.4.0
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
install(PROGRAMS "${PROJECT_SOURCE_DIR}/dwmswallow"
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(PROGRAMS "${PROJECT_SOURCE_DIR}/dwmquit"
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###
### CMake options
###

option(XSYNCHRONIZE "Use X synchronous mode (debugging)" OFF)
if(XSYNCHRONIZE)
    target_compile_definitions(dwm PUBLIC XSYNCHRONIZE)
endif()

# TODO: Add all dependencies to cmake
# TODO: Add make target for development (symlink instead of installs)
