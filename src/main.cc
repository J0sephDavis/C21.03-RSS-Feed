#include <curlpp/Exception.hpp>
#include <curlpp/OptionBase.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <rapidxml_print.hpp>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
namespace rx = rapidxml;
#include "regex.c"
//
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;
#include <chrono> //for logging
#include <ctime> //for logging

#define CONFIG_NAME "rss-config.xml"
#define LOG_NAME "rssFeed.log"
enum logLevel_t {logDEBUG, logINFO, logWARNING, logERROR};
//Modified from: https://drdobbs.com/cpp/logging-in-c/201804215
class logger {
	public:
		logger(logLevel_t level = logINFO):
			loggerLevel(level),
			start(std::chrono::system_clock::now())
		{};
		~logger() {
			os << std::endl; //flush
			std::ofstream logFile(LOG_NAME);
			logFile << os.str();
			logFile.close();
			(void)loggerLevel;
		}
		/*
		 * instead of send(msg, level) -> debug(msg), error(msg), info(msg), warning(msg) ?
		 * */
		void send(std::string message, logLevel_t level = logINFO) {
			//only print if we are >= to the logger level
			if (level >= loggerLevel)
				get(level) << message;
		}
	private:
		//todo something with the get function
		std::ostringstream& get(logLevel_t level) {
			std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			//each log will be "TIME LEVEL MESSAGE"
			os << "\n-" << std::put_time(std::localtime(&time),"%c");
			os << "[" << levelString(level)  << "]:\t";
			return os;
		}
		std::string levelString(logLevel_t level) {
			if (level == logDEBUG)
				return "DEBUG";
			if (level == logINFO)
				return "INFO";
			if (level == logWARNING)
				return "WARNING";
			if (level == logERROR)
				return "ERROR";
			return "unknown";
		}
	private:
		logLevel_t loggerLevel;
		std::chrono::time_point<std::chrono::system_clock> start;
		std::ostringstream os;
};

//TODO: better quit functions & logging, for what has been done by the program
//TODO: make an easy way for the user to update the config
//TODO: maintain a list of files that failed to download. Try to download them again before logging them and quitting
//TODO: remove regex? might not be needed if the RSS is filtered right
//TODO: when the program exits make sure it updated the conig with all the downloaded files

//used with curl to download the file
static size_t write_data(char *ptr, size_t size, size_t nmemb, void *stream) {
	size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
	return written;
}
//downloads a file
bool downloadFile(std::string url, std::string fileName, logger &log) {
	log.send("downloadFile(" + url + "," + fileName + ")", logDEBUG);
//TODO: handle fclose() & returns in a clean manner
	FILE* pagefile = fopen(fileName.c_str(), "wb");
	try {
		//set options, URL, Write callback function, progress output, &c
		curlpp::Easy request;
		request.setOpt<curlpp::options::Url>(url);
		request.setOpt<curlpp::options::NoProgress>(true);
		request.setOpt(curlpp::options::WriteFunctionCurlFunction(write_data));
		//write to the file
		if (pagefile) {
			request.setOpt<curlpp::OptionTrait<void*, CURLOPT_WRITEDATA>>(pagefile);
			request.perform();
			fclose(pagefile);
			return true;
		}
		else {
			perror("Error");
			fclose(pagefile);
			return false;
		}
	}
	catch (curlpp::LogicError &e) {
		//log.send("Failed to download file: " + std::string(e.what()), logERROR);
		std::cout << e.what() << std::endl /*endl to flush the stream, might matter*/;
	}
	catch (curlpp::RuntimeError &e) {
		//log.send("Failed to download file: " + std::string(e.what()), logERROR);
		std::cout << e.what() << std::endl;
	}
	fclose(pagefile);
	return false;
}

//returns whether the given title matches the expression
bool match_title(std::string Regular_Expression, std::string title) {
	regex expression = re_create_f_str(Regular_Expression.c_str());
	bool retval = match(expression, title.c_str());
	while(expression) {
		regex next_inst;
		if (re_getChild(expression))
			next_inst = re_getChild(expression);
		else if (re_getAlternate(expression))
			next_inst = re_getAlternate(expression);
		else
			next_inst = re_getNext(expression);
		re_destroy(expression);
		expression = next_inst;
	}
	return retval;
}
//Data we need:
//Per title: (each an object in the XML file or possibly their own XML files) 
// 	1. The regular expression to match
// 	2. The History of downloaded filed (possibly just the last one)
// 	3. The RSS feed to download & look into
/* TODO: make an xml.dtd
 * <xml ...> //HEADER
 * <root>
	<configuration>
		<download-folder>./downloads/</download-folder>
		<rss-feed-folder>./rss_feeds/</rss-feed-folder>
	</configuration>
 * 	<item>
 * 		<title>title of the feed or whatever - this is probably just for us</title>
 * 		<feedFileName>FileName</feedFileName>
 * 		<feed-url><![CDATA[https://examples.com/RSS]]</feed-url>
 * 		<expr>
 * 		<history>title of the last downloaded rss entry</history>
 * 	</item>
 * 	...ad infinitum
 * </root>
 * */
int main(void) {
	//static to 
	static logger log(logDEBUG);
	log.send("Starting Main:", logINFO);
	//NAME, URL
	std::vector<std::tuple<std::string, std::string>> download_links;
	std::string download_prefix = "./downloads/";
	std::string rss_feed_prefix = "./rss_feeds/";
	log.send("DOWNLOAD_PREFIX:" + download_prefix, logDEBUG);
	log.send("RSS_FEED_PREFIX:" + rss_feed_prefix, logDEBUG);
	if (!fs::exists(CONFIG_NAME)) {
		std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
		log.send("CONFIG: " + std::string(CONFIG_NAME) + "does not exist. quitting...", logERROR);
		exit(EXIT_FAILURE);
	}
	rx::xml_document<> config_document;
	rx::file<> config_file(CONFIG_NAME);
	try {
		log.send("Parsing config document", logDEBUG);
		config_document.parse<0>(config_file.data());
	}
	catch (rx::parse_error &e) {
		log.send("Failed to parse config:" + std::string(e.what()), logERROR);
		std::cout << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}
	//pointers to each config node
	std::vector<rx::xml_node<char>*> config_nodes;
	log.send("Traversing configuration document & storing each child node.", logDEBUG);
	for (auto item_node = config_document.first_node()->first_node("item");
			item_node;
			item_node = item_node->next_sibling()) {
		log.send(" * Adding config node", logDEBUG);
		config_nodes.push_back(item_node);
	}
	log.send("For-each config in config_nodes", logINFO);
	for (auto config : config_nodes) {
		log.send("* processing config", logINFO);
	//1. download its linked RSS feed & set some data
		std::string configFeedName = rss_feed_prefix + config->first_node("feedFileName")->value();
		std::string url = config->first_node("feed-url")->first_node()->value();
		log.send("config feed name:" + configFeedName, logDEBUG);
		log.send("feed-url:" + url, logDEBUG);
		if(!downloadFile(url, configFeedName, log)) {
			log.send("Failed to download file. SKIPPING", logERROR);
			continue;
		}
		std::string feedHistory = config->first_node("history")->value();
		log.send("history:" + feedHistory, logDEBUG);
	//2. parse the FEED
		rx::xml_document<> feed;
		rx::file<char> feedFile(configFeedName);
		try {
			log.send("Parsing the downloaded RSS", logINFO);
			feed.parse<rx::parse_no_data_nodes>(feedFile.data());
		} catch(rx::parse_error &e) {
			log.send("Failed to parse" + std::string(e.what()), logERROR);
			feed.clear();
			log.send("Skipping this feed", logINFO);
			continue;
		}
	//3. Store download links & names to config
		char* regexpression = config->first_node("expr")->value();
		log.send("Regular Expression:" + std::string(regexpression), logDEBUG);
		std::string newHistory;
		log.send("for each node in the RSS", logINFO);
		for (auto *node = feed.first_node()->first_node()->first_node("item");
				node;
				node = node->next_sibling()) {
			log.send("*node", logINFO);
			std::string entry_title = node->first_node("title")->value();
			log.send("entry_title: " + entry_title, logDEBUG);
			std::string entry_url = node->first_node("link")->value();
			log.send("entry_url: " + entry_url, logDEBUG);
			if (entry_title.compare(feedHistory) == 0) {
				log.send("entry title matches history. STOP READING", logINFO);
				break; //because it is in chronological order, we can just stop here
			}
			if (match_title(regexpression, entry_title)) {
				if (newHistory.empty()) {
					log.send("update RSS history with the top-most entry downloaded",logDEBUG);
					newHistory = entry_title;
				}
				log.send("entry added to downloads", logINFO);
				download_links.push_back({entry_title, entry_url});
			}
		}
		//TODO make sure this is the correct way to allocate a string to replace an old value
		if (!newHistory.empty()) {
			config->first_node("history")->value(config_document.allocate_string(newHistory.c_str()));
			log.send("updated the history node", logDEBUG);
		}
		feed.clear();
		log.send("finished with feed",logDEBUG);
	}
	//download the files
	log.send("processing DOWNLOADS(" + std::to_string(download_links.size()) + ")", logINFO);
	for (auto download : download_links) {
		log.send("filename BEFORE linting:" + std::get<0>(download),logDEBUG);
	//lint the string
		auto iterator = std::get<0>(download).find("https://");
		if (iterator != std::string::npos)
			std::get<0>(download).replace(iterator, 8,"");
		iterator = std::get<0>(download).find('/');
		for (;iterator != std::string::npos; iterator = std::get<0>(download).find('/'))
			std::get<0>(download).replace(iterator, 1, "");
		iterator = std::get<0>(download).find(".mkv");
		if (iterator != std::string::npos)
			std::get<0>(download).replace(iterator,4,".download");
		log.send("filename AFTER linting:" + std::get<0>(download),logDEBUG);
	//download
		std::string fileName = download_prefix + std::get<0>(download);
		if (!downloadFile(std::get<1>(download), fileName, log)) {
			log.send("failed to download file, skipping...", logERROR);
			continue;
		}
	}
	//update the config
	log.send("updating the config file", logINFO);
	std::ofstream new_xml(CONFIG_NAME);
	new_xml << config_document;
	new_xml.close();
	config_document.clear();
	log.send("exiting...", logINFO);
	std::cout << "exit\n";
	exit(EXIT_SUCCESS);
}
