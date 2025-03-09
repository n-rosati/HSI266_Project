cmake_minimum_required(VERSION 3.30)
project(Project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

add_executable(Project src/main.c
        include/main.h
)

include_directories("include")

# LabJack drivers
include_directories("C:\\Program Files (x86)\\LabJack\\Drivers")
target_link_libraries(Project "C:\\Program Files (x86)\\LabJack\\Drivers\\LabJackUD.lib")