cmake_minimum_required(VERSION 3.2)
project(AtemCHOP)

include_directories(include)

add_library(AtemCHOP MODULE src/main.cpp src/udp.cpp)
