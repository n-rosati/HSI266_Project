cmake_minimum_required(VERSION 3.30)
project(Project C)

set(CMAKE_C_STANDARD 11)

add_executable(Project main.c)

include_directories("C:\\Program Files (x86)\\LabJack\\Drivers")
target_link_libraries(Project "C:\\Program Files (x86)\\LabJack\\Drivers\\LabJackUD.lib")