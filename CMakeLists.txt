cmake_minimum_required(VERSION 3.30)
project(Project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

add_executable(Project src/main.c
        include/main.h
)

include_directories("include")

# WinSCP scripts
add_custom_target(copy-scripts ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/scripts ${CMAKE_BINARY_DIR}/scripts)
add_dependencies(Project copy-scripts)

# HTML/JS files
add_custom_target(copy-web ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/web ${CMAKE_BINARY_DIR}/web)
add_dependencies(Project copy-web)

# LabJack drivers
include_directories("C:\\Program Files (x86)\\LabJack\\Drivers")
target_link_libraries(Project "C:\\Program Files (x86)\\LabJack\\Drivers\\LabJackUD.lib")