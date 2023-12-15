#ifndef RSS_DOWNLOAD_HANDLER
#define RSS_DOWNLOAD_HANDLER

#include <condition_variable>
#include <config.h>
//
#include <iostream>
#include <logger.hpp>
#include <mutex>
#include <regex.c>
#include <utils.cpp>
//
#include <rapidxml.hpp>
#include <rapidxml_print.hpp>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
//
#include <curl/multi.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Multi.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/OptionBase.hpp>
#include <curlpp/Exception.hpp>
//
#include <thread>

namespace rssfeed {
class download_manager;
class feed;
class download_base;
//BEGIN NAMESPACE
/*
 * 	DOWNLOAD_BASE
 * */
class download_base {
	public:
		download_base(std::string url,fs::path download_path);
		~download_base();
		//downloads a file
		bool fetch();
		const fs::path getPath();
		const std::string getURL();
	protected:
		logger &log;
		const std::string url;
		fs::path filePath;
};
/*
 * 	FEED
 * */
class feed : public download_base {
	public:
		feed(rapidxml::xml_node<>& config_ptr, std::string fileName, std::string url, char* regex, std::string history);
		std::vector<download_base> parse();
		const rapidxml::xml_node<>& getConfigRef();
		bool isNewHistory();
		const std::string getHistory();
	private:
		const rapidxml::xml_node<>& config_ref;
		bool newHistory = false;
		std::string newHistoryTitle;
		const std::string feedHistory;
		const char* regexpression;
		//prevent the main function from being run twice
		bool doneParsing = false;
};
/*
 * 	DOWNLOAD_MANAGER
 * */
class download_manager {
	public:
		static download_manager& getInstance();
		 download_manager(download_manager const &) = delete;
		 void operator=(download_manager const &) = delete;
		~download_manager();
		//
		void add(download_base &download);
		void multirun();
		//
	private:
		download_manager();
		std::vector<std::pair<curlpp::Easy *, FILE *>> getRequests(size_t num_get = 4);
		//
		std::queue<download_base>  downloads;
		std::mutex queue_lock;
		logger& log;
};
//END NAMESPACE
}
#endif
