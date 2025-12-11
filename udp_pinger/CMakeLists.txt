cmake_minimum_required(VERSION 3.31)
project(udp_pinger)

set(CMAKE_CXX_STANDARD 20)

add_executable(udp_pinger_server udp_pinger_server.cpp)

add_executable(udp_pinger_client udp_pinger_client.cpp)