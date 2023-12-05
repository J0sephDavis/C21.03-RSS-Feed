cmake_minimum_required (VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

project(RSS-Feed
  LANGUAGES CXX
  VERSION 1.0.0
)
add_executable(RSS-Feed 
	src/main.cc
)

target_include_directories(RSS-Feed
	PRIVATE src rapidxml-1.13
)

target_link_libraries(RSS-Feed 
	PRIVATE curlpp
	INTERFACE rapidxml
)
set_target_properties(RSS-Feed PROPERTIES CXX_STANDARD 20)
#fetchContent & find_package failed to properly link libcurlpp....
target_compile_options(RSS-Feed PRIVATE -Wall -Werror -Wextra -Wpedantic -lcurlpp)
