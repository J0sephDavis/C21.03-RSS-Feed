cmake_minimum_required (VERSION 3.11)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

project(rapidxml
  LANGUAGES CXX
  VERSION 1.13
  DESCRIPTION "Header library for reading XML files. from: https://rapidxml.sourceforge.net/"
)

add_library(rapidxml INTERFACE)
target_include_directories(rapidxml INTERFACE .)
