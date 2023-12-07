cmake_minimum_required (VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

project(RSS-Feed
  LANGUAGES CXX
  VERSION 1.0.6
)
add_executable(RSS-Feed 
	src/main.cc
)
find_package( CURL REQUIRED )

target_include_directories(RSS-Feed
	PRIVATE src rapidxml-1.13 curlpp_v81
)

target_link_libraries(RSS-Feed 
	INTERFACE rapidxml
	PUBLIC CURL::libcurl
	PUBLIC curlpp
)
option(USE_PRINT_LOGS, "Print Logs to STDOUT")
if (USE_PRINT_LOGS)
	add_compile_definitions(PRINT_LOGS=ON)
endif()
set_target_properties(RSS-Feed PROPERTIES CXX_STANDARD 20)
#fetchContent & find_package failed to properly link libcurlpp....
target_compile_options(RSS-Feed PRIVATE -Wall -Werror -Wextra -Wpedantic)
