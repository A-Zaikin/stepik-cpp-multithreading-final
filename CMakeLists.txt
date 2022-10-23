cmake_minimum_required(VERSION 2.8)
project(final)

set(CMAKE_CXX_STANDARD 14)

add_executable(final main.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")