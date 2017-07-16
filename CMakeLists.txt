cmake_minimum_required(VERSION 3.6)
project(tcp_tunnel)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp unp.h error_check.cpp ofmsg.cpp ofmsg.h openflow_common.h schedule.cpp schedule.h)
add_executable(tcp_tunnel ${SOURCE_FILES})