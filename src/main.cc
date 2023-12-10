#include <mutex>
#include <queue>
#include <thread>
#include <csignal> //reference for SIGINT & SIGSEGV
#include <config.h>
#include <curlpp/Exception.hpp>
#include <curlpp/OptionBase.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <rapidxml_print.hpp>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include <regex.c>
//
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <iostream>
#include <filesystem>
#include <chrono> //for logging
#include <ctime> //for logging

//TODO set these directory & foler names in config.h.in
#define CONFIG_NAME "rss-config.xml"
#define RSS_FOLDER "./rss_feeds/"
#define DOWNLOAD_FOLDER "./downloads/"
#define LOG_FOLDER "./logs/"
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

//TODO: make an easy way for the user to update the config
//TODO: maintain a list of files that failed to download. Try to download them again before logging them and quitting
//TODO: remove regex? might not be needed if the RSS is filtered right
//TODO: multi-thread filedownloads
//TODO: modify the download class to only enforce on rssContents, not when we download other files(like feeds which must be clobbered)

namespace rssfeed {
/*-------------------- BEGIN RSS FEED NAMESPACE --------------------*/
namespace fs = std::filesystem;
namespace rx = rapidxml;
enum logLevel_t {
	logDEBUG, 	//
	logTRACE, 	//trace exeuction across project. What logic was done
	logWARNING, 	//Things that might have caused problems but don't affect the end functionality of the system
	logINFO, 	//In gist, what has been/is being done
	logERROR 	//a real problem that is detrimental to the functionality of the system.
};
class logger {
	//https://stackoverflow.com/questions/1008019/how-do-you-implement-the-singleton-design-pattern
	//Modified from: https://drdobbs.com/cpp/logging-in-c/201804215
	//TODO: queue with mutext for multiple producers (of input) & one consumer (the logger as it writes)
	//TODO: have log file update during program runtime, not just when it exits successfully...
	public:
		static logger& getInstance(logLevel_t level = logINFO) {
			//https://stackoverflow.com/questions/335369/finding-c-static-initialization-order-problems/335746#335746
			//Take a look at destruction problems in that thread
			static logger instance(level);
			instance.send("getInstance()", logTRACE);
			return instance;
		}
		 logger(logger const &) = delete;
		 void operator=(logger const &) = delete;
		~logger() {
			send("DESTRUCT LOGGER", logTRACE);
			os << std::endl; //flush
			std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			const std::string log_prefix = "rss-feed-";
			const std::string log_suffix = ".log";
#define logs_folder_len 7
#define timestamp_len 16
			std::string datetime(logs_folder_len + timestamp_len + log_prefix.size() + log_suffix.size() + 10
					,0
			);
			//https://stackoverflow.com/questions/28977585/how-to-get-put-time-into-a-variable
			datetime.resize(std::strftime(&datetime[0], datetime.size(), "%Y-%m-%d_%H-%M", std::localtime(&time)));
			std::string fileName = LOG_FOLDER + log_prefix + datetime + log_suffix;
			if(!fs::exists(LOG_FOLDER)) {
				//we don't catch the error, let it fail...
				fs::create_directory(LOG_FOLDER);
			}
			//
			std::cout << "log file=" << fileName << std::endl;
			std::ofstream logFile(fileName);
			clear_queue();
			logFile << os.str();
			logFile.close();
		}
	private:
		logger(logLevel_t level):
			loggerLevel(level) {
				send("CONSTRUCT LOGGER", logTRACE);
				send("VERSION" + std::string(PROJECT_VERSION));
			};
	private:
		std::string levelString(logLevel_t level) {
			if (level == logTRACE)
				return "TRACE";
			if (level == logDEBUG)
				return "DEBUG";
			if (level == logINFO)
				return "INFO";
			if (level == logWARNING)
				return "WARNING";
			if (level == logERROR)
				return "ERROR";
			return "unknown-loglevel";
		}
		void clear_queue() {
			std::lock_guard lock(queue_write);
			for (; !messages.empty(); messages.pop()) {
				auto& msg = messages.front();
				os << msg.str();
//				std::cout << "test:" << msg.str();
			}
		}
	private:
		logLevel_t loggerLevel;
		//the actual output stream
		std::ostringstream os;
		std::queue<std::ostringstream>  messages;
		std::mutex queue_write;
	public:
		//sends a message to the log
		void send(std::string message, logLevel_t level = logINFO) {
			if (messages.size() > 10) clear_queue();
			//only print if we are >= to the logger level
			if (level >= loggerLevel) {
				std::lock_guard lock(queue_write); //released when function ends
				std::ostringstream tmp_output;
				std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				//each log will be "TIME LEVEL MESSAGE"
				tmp_output << "\n-" << std::put_time(std::localtime(&time),"%c");
				tmp_output << "[" << levelString(level)  << "]:\t";
				tmp_output << message;
//#if COUT_LOG
//				std::cout << "\n" + tmp_output;
//#endif
				messages.push(std::move(tmp_output));
			}
		}

};
static size_t write_data(char *ptr, size_t size, size_t nmemb, void *stream) {
//	static logger& log = logger::getInstance();
//	log.send("WRITE_DATA",logTRACE);
	size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
	return written;
}

//returns whether the given title matches the expression
bool match_title(std::string Regular_Expression, std::string title) {
	static logger& log = logger::getInstance();
	log.send("match_title", logTRACE);
	log.send("expression: "  + Regular_Expression +"\ttitle:" + title, logDEBUG);
	regex expression = re_create_f_str(Regular_Expression.c_str());
	bool retval = match(expression, title.c_str());
	//this is to destroy the expression... I really ought to edit C14.02 and move this into a function there
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
	log.send("match_title return:" + std::string((retval)?"true":"false"), logDEBUG);
	return retval;
}


std::string url_to_filename(const std::string url) {
	std::string fileName = url;
	const std::string url_start = "https://";
	auto iterator = fileName.find(url_start);
	fileName.replace(iterator,url_start.size(),"");
	iterator = fileName.find('/');
	while (iterator != std::string::npos) {
		fileName.replace(iterator,1,"");
		iterator = fileName.find('/');
	}
	return fileName;
}
//downloads a file from a url to the given path(includes filename)
class download_base {
	public:
		download_base(std::string url,fs::path download_path) :
			log(logger::getInstance()),
			url(url)
		{
			filePath = download_path;
			log.send("Called DOWNLOAD_ENTRY constructor", logTRACE);
			log.send("called with:\n\turl:" + url + "\n\tdownload_path:" + download_path.string(), logDEBUG);
		};
		~download_base() {
			log.send("DOWNLOAD_ENTRY destroyed",logTRACE);
		}
		//downloads a file
		bool fetch() {
			log.send("fetch", logTRACE);
			log.send("url:" + url + "\tpath: " + filePath.string(), logDEBUG);
			if(fs::exists(filePath)) {
				log.send("FILE ALREADY EXISTS", logWARNING);
#if !CLOBBER_FLAG
				return false;
#endif
			}
			//TODO: handle fclose() & returns in a clean manner
			//this section of code somewhat irks me. Not the curlpp,
			//but the error handling & that we are using a FILE* rather than an fstream.
			//Must look into how we can get to an fstream with curlpp write callback
			FILE* pagefile = fopen(filePath.c_str(), "wb");
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
					log.send("File downloaded successfully");
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
		const fs::path getPath() {
			return filePath;
		}
	private:
		logger& log;
		const std::string url;
		fs::path filePath;
};
class downloadManager {
	//TODO: have log file update during program runtime, not just when it exits successfully...
	public:
		static downloadManager& getInstance()
		{
			static downloadManager instance;
			return instance;
		}
		 downloadManager(downloadManager const &) = delete;
		 void operator=(downloadManager const &) = delete;
		~downloadManager() {
			log.send("DESTRUCT downloadManager", logTRACE);
		}
	private:
		downloadManager():
		log(logger::getInstance())
		{
			log.send("CONSTRUCT downloadManager", logTRACE);
		};
	private:
		std::queue<download_base>  downloads;
		std::mutex queue_write;
		logger& log;
	public:
		void add(std::string url, fs::path filePath) {
			std::lock_guard lock(queue_write); //released when function ends
			log.send("add()",logTRACE);
			log.send("path:" + filePath.string() + ", url: " + url, logDEBUG);
			downloads.emplace(url, filePath);
		}
		void run() {
			std::lock_guard lock(queue_write);
			while (!downloads.empty()) {
				downloads.front().fetch();
				downloads.pop();
			}
		}
};
//checks if a folder exists, if not it attempts to create the folder. returns true on success
bool createFolderIfNotExist(fs::path folder) {
	static logger& log = logger::getInstance();
	if(!fs::exists(folder)) {
		log.send(folder.string() + " does not exist. Attempting to create.", logWARNING);
		try {
			fs::create_directory(folder);
			log.send("Created folder", logINFO);
		} catch (fs::filesystem_error &e) {
			log.send("Failed to create folder(" + folder.string() + ")\n" + e.what(),logERROR);
			return false;
		}
	}
	return true;
}
/*-------------------- END RSS FEED NAMESPACE --------------------*/
}
using namespace rssfeed;
//
static void signal_handler(int signal) {
	static logger &log = logger::getInstance();
	log.send("Signal(" + std::to_string(signal) + ") raised. quitting...", logERROR);
	std::cout << "signal(" << signal << ") received: quitting\n";
	exit(EXIT_FAILURE);
}
class feed : private download_base {
	public:
		feed(rx::xml_node<>& config_ptr, std::string fileName, std::string url, char* regex, std::string history):
			download_base(url, RSS_FOLDER + fileName),
			config_ref(config_ptr),
			log(logger::getInstance()),
			downloads(downloadManager::getInstance()),
			feedHistory(history),
			regexpression(regex)
		{
			log.send("feed constructor",logTRACE);
			log.send("NAME:" + this->getPath().string());
			log.send("URL:" + url, logDEBUG);
			log.send("HISTORY:" + feedHistory);
			if (!fetch()) throw std::runtime_error("Failed to download file");
		};
		void parse() {
			log.send("PARSE_THREAD()", logTRACE);
			if (doneParsing) return;
		//2. parse the FEED
			rx::xml_document<> feed;
			rx::file<char> feedFile(getPath());
			try {
				log.send("Parsing RSS", logTRACE);
				feed.parse<rx::parse_no_data_nodes>(feedFile.data());
			} catch(rx::parse_error &e) {
				log.send("Failed to parse" + std::string(e.what()) + "\nskipping...", logERROR);
				feed.clear();
				throw std::runtime_error("Failed to parse file");
			}
		//3. Store download links & names to config
			log.send("Regular Expression:" + std::string(regexpression), logDEBUG);
			log.send("Begin fetching downloads", logTRACE);
			for (auto *node = feed.first_node()->first_node()->first_node("item");
					node;
					node = node->next_sibling()) {
				std::string entry_title = node->first_node("title")->value();
				log.send("ENTRY TITLE: " + entry_title, logDEBUG);
				std::string entry_url = node->first_node("link")->value();
				log.send("URL: " + entry_url, logDEBUG);
				if (entry_title.compare(feedHistory) == 0) {
					log.send("Download in history. skipping...", logTRACE);
					break; //because it is in chronological order, we can just stop here
				}
				if (match_title(regexpression, entry_title)) {
					if (newHistory == false) {
						newHistory = true;
						newHistoryTitle = entry_title;
						log.send("set new history", logTRACE);
					}
					downloads.add(entry_url, fs::path(DOWNLOAD_FOLDER + url_to_filename(entry_url)));
					log.send("Added " + entry_title + " to downloads");
				}
			}
			feed.clear();
			doneParsing = true;
		}
		//returns the pointer to the xml child in the config relating to this feed
		const rx::xml_node<>& getConfigRef() {
			return config_ref;
		}
		bool isNewHistory() {
			log.send("isNewHistory()",logTRACE);
			return newHistory;
		}
		//if downloads != empty, then the history received MUST be new
		const std::string getHistory() {
			if (newHistory) return newHistoryTitle;
			else return feedHistory;
		}
	private:
		const rx::xml_node<>& config_ref;
		logger& log;
		downloadManager& downloads;
	private:
		bool newHistory = false;
		std::string newHistoryTitle;
		const std::string feedHistory;
		const char* regexpression;
		//prevent the main function from being run twice
		bool doneParsing = false;
};

int main(void) {
	static logger &log = logger::getInstance(logDEBUG);
	if (!fs::exists(CONFIG_NAME)) {
		std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
		log.send("CONFIG: " + std::string(CONFIG_NAME) + "does not exist", logERROR);
		log.send("Quitting...");
		exit(EXIT_FAILURE);
	}
	if(!createFolderIfNotExist(RSS_FOLDER)) {
		log.send("Quitting...");
		exit(EXIT_FAILURE);
	}
	if(!createFolderIfNotExist(DOWNLOAD_FOLDER)) {
		log.send("Quitting...");
		exit(EXIT_FAILURE);
	}
	//signal handlers that ensure we properly deconstruct our static variables on exit
	std::signal(SIGSEGV, signal_handler);
	std::signal(SIGINT, signal_handler);
	//loads the files... they both must live together
	static rx::xml_document<> config_document;
	static rx::file<> config_file(CONFIG_NAME);
	try {
		log.send("Parsing config document", logTRACE);
		config_document.parse<0>(config_file.data());
	}
	catch (rx::parse_error &e) {
		log.send("Failed to parse config:" + std::string(e.what()), logERROR);
		exit(EXIT_FAILURE);
	}
	//pointers to each config node
	std::vector<feed> feeds;
	log.send("Collect entries from the config", logTRACE);
	for (auto item_node = config_document.first_node()->first_node("item");
			item_node;
			item_node = item_node->next_sibling()) {
		std::string configFeedName = item_node->first_node("feedFileName")->value();
		std::string url = item_node->first_node("feed-url")->first_node()->value();
		std::string feedHistory = item_node->first_node("history")->value();
		char* regexpression = item_node->first_node("expr")->value();
		try {
			auto tmp = feed(*item_node, configFeedName, url, regexpression, feedHistory);
			feeds.emplace_back(std::move(tmp));
			log.send("Added feed() to vector", logTRACE);
		} catch(std::runtime_error &e) {
			log.send(e.what(), logERROR);
		}
	}
	std::vector<std::thread> parsing_feeds;
	//parse each feed in a thread
	for (auto& current_feed : feeds) {
		log.send("creating thread",logTRACE);
		auto t = std::thread(&feed::parse,std::ref(current_feed));
		parsing_feeds.push_back(std::move(t));
	}
	//join threads
	log.send("joining threads");
	for (auto& t : parsing_feeds)
		t.join();
	//sequentially download each file
	downloadManager::getInstance().run();
	log.send("checking feeds for updated histories", logTRACE);
	for (auto& current_feed : feeds) {
		if (current_feed.isNewHistory()) {
			current_feed.getConfigRef().first_node("history")->first_node()->value(config_document.allocate_string(current_feed.getHistory().c_str()));
			log.send("Updated history",logTRACE);
			log.send("newHistory:" + current_feed.getHistory(),logDEBUG);
		}
	}
	//update the config
	log.send("Saving the updated config file", logTRACE);
	std::ofstream new_xml(CONFIG_NAME);
	new_xml << config_document;
	new_xml.close();
	log.send("Config file saved", logTRACE);
	config_document.clear();
	exit(EXIT_SUCCESS);
}
