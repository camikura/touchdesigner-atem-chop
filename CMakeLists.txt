cmake_minimum_required(VERSION 3.2)
project(AtemCHOP)

include_directories(include)

add_library(AtemCHOP MODULE src/main.cc src/atem.cc src/udp.cc)

if(MSVC)
  target_compile_options(AtemCHOP PUBLIC "/std:c++latest")
endif()
