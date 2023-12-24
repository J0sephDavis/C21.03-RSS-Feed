cmake_minimum_required (VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

project(RSS-Feed
  LANGUAGES CXX
  VERSION 1.1.3
)
add_executable(RSS-Feed 
	main.cc
)


add_subdirectory(c1402-regex)
add_subdirectory(curlpp_v81)
target_include_directories(RSS-Feed
	PRIVATE . rapidxml-1.13 curlpp_v81 c1402-regex
	"${PROJECT_BINARY_DIR}"
)
target_link_libraries(RSS-Feed 
	PRIVATE curlpp
		regex
	INTERFACE rapidxml
)

option(PRINT_LOGS "Print Logs to STDOUT" OFF)
if (PRINT_LOGS)
	message("Print is on")
	target_compile_definitions(RSS-Feed PRIVATE COUT_LOG=1)
else()
	message("Print is off")
	target_compile_definitions(RSS-Feed PRIVATE COUT_LOG=0)
endif()

#for config.h
set(LOG_FOLDER "${PROJECT_BINARY_DIR}/../data/logs/")
set(RSS_FOLDER "${PROJECT_BINARY_DIR}/../data/rss_feeds/")
set(DOWNLOAD_FOLDER "${PROJECT_BINARY_DIR}/../data/downloads/")
set(CONFIG_NAME "${PROJECT_BINARY_DIR}/../data/rss-config.xml")
configure_file("config.h.in" "config.h" @ONLY)
#https://stackoverflow.com/questions/34143265/undefined-reference-to-symbol-pthread-createglibc-2-2-5
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set_target_properties(RSS-Feed PROPERTIES CXX_STANDARD 20)
target_compile_options(RSS-Feed PRIVATE -Werror -Wextra -Wpedantic -lpthread)
