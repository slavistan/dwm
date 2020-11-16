cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_DISABLE_SOURCE_CHANGES on)
set(CMAKE_DISABLE_IN_SOURCE_BUILD on)

project(dwm
    VERSION 0.0.1
    LANGUAGES C)

find_package(X11 REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)

include_directories(/usr/include/freetype2)

add_executable(dwm dwm.c drw.c util.c)
target_link_libraries(dwm X11 fontconfig Xft Xinerama Xrender)
install(TARGETS dwm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(dwmctl util.c dwmctl.c)
target_link_libraries(dwmctl X11)
install(TARGETS dwmctl DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

option(XSYNCHRONIZE "Use X synchronous mode (debugging)" OFF)
if(XSYNCHRONIZE)
    target_compile_definitions(dwm PUBLIC XSYNCHRONIZE)
endif()

# TODO(fix): Unfuck CMakeLists.txt
#            - [ ] Freetype header must be included manually.
#            - [ ] Built independent object files.