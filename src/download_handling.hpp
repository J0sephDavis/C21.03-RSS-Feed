#ifndef RSS_DOWNLOAD_HANDLER
#define RSS_DOWNLOAD_HANDLER

#include <config.h>
//
#include <iostream>
#include <logger.hpp>
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
//BEGIN NAMESPACE
//downloads a file from a url to the given path(includes filename)
class download_base {
	public:
		download_base(std::string url,fs::path download_path);
		~download_base();
		//downloads a file
		bool fetch();
		const fs::path getPath();
		const std::string getURL();
	private:
		logger& log;
		const std::string url;
		fs::path filePath;
};
class download_manager {
	public:
		static download_manager& getInstance();
		 download_manager(download_manager const &) = delete;
		 void operator=(download_manager const &) = delete;
		~download_manager();
	private:
		download_manager();
	private:
		std::queue<download_base>  downloads;
		std::mutex queue_write;
		logger& log;
	public:
		void add(std::string url, fs::path filePath);
		void add(download_base &download);
		//void run();
		std::vector<std::pair<curlpp::Easy *, FILE *>> getRequests(size_t num_get = 4);
		void multirun(int batchSize);
};
class feed : public download_base {
	public:
		feed(rapidxml::xml_node<>& config_ptr, std::string fileName, std::string url, char* regex, std::string history);
		void parse();
		const rapidxml::xml_node<>& getConfigRef();
		bool isNewHistory();
		const std::string getHistory();
	private:
		const rapidxml::xml_node<>& config_ref;
		logger& log;
		download_manager& downloads;
	private:
		bool newHistory = false;
		std::string newHistoryTitle;
		const std::string feedHistory;
		const char* regexpression;
		//prevent the main function from being run twice
		bool doneParsing = false;
};
//END NAMESPACE
}
#endif
