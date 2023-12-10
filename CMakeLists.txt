cmake_minimum_required (VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

project(RSS-Feed
  LANGUAGES CXX
  VERSION 1.1.0
)
add_executable(RSS-Feed 
	src/main.cc
)
find_package( CURL REQUIRED )

target_include_directories(RSS-Feed
	PRIVATE src rapidxml-1.13 curlpp_v81
	"${PROJECT_BINARY_DIR}"
)

target_link_libraries(RSS-Feed 
	INTERFACE rapidxml
	PUBLIC CURL::libcurl
	PUBLIC curlpp
)

option(PRINT_LOGS "Print Logs to STDOUT" OFF)
if (PRINT_LOGS)
	message("Print is on")
	target_compile_definitions(RSS-Feed PRIVATE COUT_LOG=1)
else()
	message("Print is off")
	target_compile_definitions(RSS-Feed PRIVATE COUT_LOG=0)
endif()

option(CLOBBER_DLS "Overwrite downloads" OFF)
if (CLOBBER_DLS)
	message("clobber is on")
	target_compile_definitions(RSS-Feed PRIVATE CLOBBER_FLAG=1)
else()
	message("clobber is off")
	target_compile_definitions(RSS-Feed PRIVATE CLOBBER_FLAG=0)
endif()
configure_file("${PROJECT_SOURCE_DIR}/src/config.h.in" "config.h" @ONLY)
#https://stackoverflow.com/questions/34143265/undefined-reference-to-symbol-pthread-createglibc-2-2-5
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set_target_properties(RSS-Feed PROPERTIES CXX_STANDARD 20)
#fetchContent & find_package failed to properly link libcurlpp....
target_compile_options(RSS-Feed PRIVATE -Wall -Werror -Wextra -Wpedantic -lpthread)
