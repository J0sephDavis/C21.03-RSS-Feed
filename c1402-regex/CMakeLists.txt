cmake_minimum_required (VERSION 3.14)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
project(regex
  LANGUAGES CXX
  VERSION 1.2.0
  DESCRIPTION "Library for regex operations"
)
add_library(regex INTERFACE
	#	C1402_regex.cc
	#	C1402_regex.h
)
target_include_directories(regex INTERFACE .)
#set_target_properties(regex PROPERTIES CXX_STANDARD 20)

#option(PRINT_MESSAGES "Print debug information" OFF)
#if (PRINT_MESSAGES)
#	message("regex:: printing enabled")
#	target_compile_definitions(regex PRIVATE PRINT_MESSAGES=1)
#else()
#	message("regex:: printing disabled")
#	target_compile_definitions(regex PRIVATE PRINT_MESSAGES=0)
#endif()
#Include(FetchContent)
#FetchContent_Declare(
#	Catch2
#	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#	GIT_TAG v3.5.0
#)
#FetchContent_MakeAvailable(Catch2)
#option(UNIT_TEST"enable unit testing" OFF)
#if (UNIT_TEST)
#	message("C1402: unit testing enabled")
#	target_compile_definitions(regex PRIVATE UNIT_TESTING)
#	target_link_libraries(regex PRIVATE Catch2::Catch2WithMain)
#else()
#	message("C1402: main enabled")
#	target_compile_definitions(regex PRIVATE REGEX_MAIN)
#endif()
#target_compile_options(regex PRIVATE -Wall -Werror -Wextra -Wpedantic -Wshadow)
