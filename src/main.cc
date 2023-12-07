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
enum logLevel_t {logDEBUG, logINFO, logWARNING, logERROR};
//Modified from: https://drdobbs.com/cpp/logging-in-c/201804215
class logger {
	public:
		logger(logLevel_t level = logINFO, bool printLogs = false):
			print_logs(printLogs),
			loggerLevel(level)
		{};
		~logger() {
			os << std::endl; //flush
			std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			const std::string log_prefix = "rss-feed-";
			const std::string log_suffix = ".log";
			//timestamp length = ~16
			//pre&postfix = 13
			std::string datetime(40,0);
			//https://stackoverflow.com/questions/28977585/how-to-get-put-time-into-a-variable
			datetime.resize(std::strftime(&datetime[0], datetime.size(), "%Y-%m-%d_%H-%M", std::localtime(&time)));
			std::string fileName = log_prefix + datetime + log_suffix;
			std::ofstream logFile(fileName);
			logFile << os.str();
			logFile.close();
		}
		/*
		 * instead of send(msg, level) -> debug(msg), error(msg), info(msg), warning(msg) ?
		 * */
		void send(std::string message, logLevel_t level = logINFO) {
			//only print if we are >= to the logger level
			if (level >= loggerLevel) {
				preface_line(level);
				os << message;
				if (print_logs) std::cout << "\n" << message;
			}
		}
	private:
		//adds a timestamp and log-stamp
		void preface_line(logLevel_t level) {
			std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			//each log will be "TIME LEVEL MESSAGE"
			os << "\n-" << std::put_time(std::localtime(&time),"%c");
			os << "[" << levelString(level)  << "]:\t";
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
		const bool print_logs;
		logLevel_t loggerLevel;
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
			return false;
		}
	}
	catch (curlpp::LogicError &e) {
		log.send("Failed to download file: " + std::string(e.what()), logERROR);
	}
	catch (curlpp::RuntimeError &e) {
		log.send("Failed to download file: " + std::string(e.what()), logERROR);
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
	static logger log(logDEBUG,false);
	log.send("Starting Main:", logINFO);
	if (!fs::exists(CONFIG_NAME)) {
		std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
		log.send("CONFIG: " + std::string(CONFIG_NAME) + "does not exist. quitting...", logERROR);
		exit(EXIT_FAILURE);
	}
	//loads the files... they both must live together
	static rx::xml_document<> config_document;
	static rx::file<> config_file(CONFIG_NAME);
	try {
		log.send("Parsing config document", logDEBUG);
		config_document.parse<0>(config_file.data());
	}
	catch (rx::parse_error &e) {
		log.send("Failed to parse config:" + std::string(e.what()), logERROR);
		exit(EXIT_FAILURE);
	}
	//
	//pointers to each config node
	std::vector<rx::xml_node<char>*> config_nodes;
	log.send("Traversing configuration document & storing each child node.", logDEBUG);
	for (auto item_node = config_document.first_node()->first_node("item");
			item_node;
			item_node = item_node->next_sibling()) {
		log.send(" * Adding config node", logDEBUG);
		config_nodes.push_back(item_node);
	}
	//downloads in format of: NAME, URL
	std::vector<std::string> download_links;
{//begin-scope
	const std::string rss_feed_prefix = "./rss_feeds/";
	log.send("RSS_FEED_PREFIX:" + rss_feed_prefix, logINFO);
	if (!fs::exists(rss_feed_prefix)) {
		try {
			log.send("Attempting to create " + rss_feed_prefix + " directory");
			fs::create_directory(rss_feed_prefix);
		} catch (fs::filesystem_error &e) {
			log.send("Failed to create directory");
			exit(EXIT_FAILURE);
		}
	}
	for (auto config : config_nodes) {
	//1. download its linked RSS feed & set some data
		std::string configFeedName = rss_feed_prefix + config->first_node("feedFileName")->value();
		std::string url = config->first_node("feed-url")->first_node()->value();
		log.send("----------", logINFO);
		log.send("NAME:" + configFeedName, logINFO);
		log.send("URL:" + url, logDEBUG);
		if(!downloadFile(url, configFeedName, log)) {
			log.send("Failed to download file. SKIPPING...", logERROR);
			continue;
		}
		std::string feedHistory = config->first_node("history")->value();
		log.send("HISTORY:" + feedHistory, logINFO);
	//2. parse the FEED
		rx::xml_document<> feed;
		rx::file<char> feedFile(configFeedName);
		try {
			log.send("Parsing RSS", logINFO);
			feed.parse<rx::parse_no_data_nodes>(feedFile.data());
		} catch(rx::parse_error &e) {
			log.send("Failed to parse" + std::string(e.what()) + "\nskipping...", logERROR);
			feed.clear();
			continue;
		}
	//3. Store download links & names to config
		char* regexpression = config->first_node("expr")->value();
		log.send("Regular Expression:" + std::string(regexpression), logDEBUG);
		std::string newHistory;
		log.send("Process RSS feed", logINFO);
		for (auto *node = feed.first_node()->first_node()->first_node("item");
				node;
				node = node->next_sibling()) {
			log.send("----------", logINFO);
			std::string entry_title = node->first_node("title")->value();
			log.send("TITLE: " + entry_title, logINFO);
			std::string entry_url = node->first_node("link")->value();
			log.send("URL: " + entry_url, logDEBUG);
			if (entry_title.compare(feedHistory) == 0) {
				log.send("TITLE matches HISTORY; NEXT FEED.", logINFO);
				break; //because it is in chronological order, we can just stop here
			}
			if (match_title(regexpression, entry_title)) {
				if (newHistory.empty()) {
					newHistory = entry_title;
					log.send("NEW-HISTORY",logDEBUG);
				}
				download_links.push_back(entry_url);
				log.send("Send to DOWNLOADS", logINFO);
			}
		}
		if (!newHistory.empty()) {
			//text-nodes are nodes themself.... this the additional .first_node() call
			//https://stackoverflow.com/questions/62785421/c-rapidxml-traversing-with-first-node-to-modify-the-value-of-a-node-in-an
			config->first_node("history")->first_node()->value(config_document.allocate_string(newHistory.c_str()));
			log.send("Allocated NEW-HISTORY node", logDEBUG);
		}
		feed.clear();
	}
}//end-scope
	//download the files
	log.send("processing DOWNLOADS(" + std::to_string(download_links.size()) + ")", logINFO);
	const std::string download_prefix = "./downloads/";
	if(!fs::exists(download_prefix)) {
		try {
			log.send("Attempting to create " + download_prefix + " directory");
			fs::create_directory(download_prefix);
		} catch (fs::filesystem_error &e) {
			log.send("Failed to create directory");
			exit(EXIT_FAILURE);
		}
	}
	log.send("DOWNLOAD_PREFIX:" + download_prefix, logINFO);
	for (auto download : download_links) {
		log.send("----------");
		std::string fileName = download;
		log.send("filename BEFORE linting:" + fileName,logDEBUG);
	//lint the string
		//we were previously saving it as the title name, but the LINK value has the proper file-format for the fileName...
		auto iterator = fileName.find("https://");
		if (iterator != std::string::npos)
			fileName.replace(iterator, 8,"");
		iterator = fileName.find('/');
		for (;iterator != std::string::npos; iterator = fileName.find('/'))
			fileName.replace(iterator, 1, "");
		log.send("filename AFTER linting:" + fileName,logDEBUG);
	//download
		fileName = download_prefix + fileName;
		if (!downloadFile(download, fileName, log)) {
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
	log.send("<--EXIT-->", logINFO);
	exit(EXIT_SUCCESS);
}
