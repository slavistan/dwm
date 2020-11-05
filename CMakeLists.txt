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
target_link_libraries(dwm
    X11 fontconfig Xft Xinerama Xrender
)