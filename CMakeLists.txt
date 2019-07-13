cmake_minimum_required(VERSION 3.14)
project(usb_send)

set(CMAKE_CXX_STANDARD 17)

add_executable(usb_send main.cpp libusbWrapper.h sendPackets.h)
target_link_libraries(usb_send usb-1.0)
