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
//TODO: multi-thread downloads & processing

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
		//adds a timestamp and log-stamp
		void preface_line(logLevel_t level) {
			std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			//each log will be "TIME LEVEL MESSAGE"
			os << "\n-" << std::put_time(std::localtime(&time),"%c");
			os << "[" << levelString(level)  << "]:\t";
		}
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
	private:
		logLevel_t loggerLevel;
		std::ostringstream os;
	public:
		//sends a message to the log
		void send(std::string message, logLevel_t level = logINFO) {
			//only print if we are >= to the logger level
			if (level >= loggerLevel) {
				preface_line(level);
				os << message;
#if COUT_LOG
				std::cout << "\n" << message;
#endif
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


//TODO: Single-respoinsibility, stop making this class handle configs & other functionaility...
/* prepares to download a file given a url, path, & optionally a file name.
 * If a file name is given, save to downloadPath/fileName
 * If no file name is given, lint the url and make it the filename (removing slashes or other symbols)
*/
class download_entry {
	public:
		download_entry(std::string url,std::string download_path) :
			log(logger::getInstance()),
			url(url)
		{
			log.send("No fileName provided, creating from linted url", logTRACE);
			fileName = url;

			const std::string url_start = "https://";
			auto iterator = fileName.find(url_start);
			fileName.replace(iterator,url_start.size(),"");
			log.send("\tfileName: " + fileName, logDEBUG);
			iterator = fileName.find('/');
			while (iterator != std::string::npos) {
				fileName.replace(iterator,1,"");
				iterator = fileName.find('/');
			}

			fileName.insert(0,download_path);
			log.send("\tfileName: " + fileName, logDEBUG);
		}
		download_entry(std::string url,std::string download_path, std::string name_of_file):
			log(logger::getInstance()),
			url(url)
		{
			log.send("Called DOWNLOAD_ENTRY constructor", logTRACE);
			log.send("called with:\n\turl:" + url + "\n\tdownload_path:" + download_path + "\n\tname:" + name_of_file, logDEBUG);
			fileName = download_path + name_of_file;
			log.send("created with:\n\turl:" + url + "\n\tname:" + fileName, logDEBUG);
		};
		~download_entry() {
			log.send("DOWNLOAD_ENTRY destroyed with:\n\turl:" + url + "\n\tname:" + fileName, logDEBUG);
		}
		//downloads a file
		bool fetch() {
			log.send("fetch", logTRACE);
			log.send("url:" + url + "\tfileName: " + fileName, logDEBUG);
			if(fs::exists(fileName)) {
				log.send("FILE ALREADY EXISTS", logWARNING);
#if !CLOBBER_FLAG
				return false;
#endif
			}
			//TODO: handle fclose() & returns in a clean manner
			//this section of code somewhat irks me. Not the curlpp,
			//but the error handling & that we are using a FILE* rather than an fstream.
			//Must look into how we can get to an fstream with curlpp write callback
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
		const std::string getFileName() {
			return fileName;
		}
	private:
		logger& log;
		const std::string url;
		std::string fileName;
};
//TODO: determine a new name for this class
/* Update the config node on a successful download, IF title != NULL
*/
class rssContents : private download_entry {
	public:
//download_entry(logger &log, std::string url,std::string download_path, std::string name_of_file = "") :
		rssContents(rx::xml_node<> &config, char* title, std::string url, std::string download_folder) :
			download_entry(url, download_folder),
			log(logger::getInstance()),
			allocated_title(title),
			associated_config(config)
		{
			log.send("rssContents constructor", logTRACE);
			if (title != NULL)
				log.send("title =" + std::string(title), logDEBUG);
			log.send("url:" + url + ", folder:" + download_folder, logDEBUG);
		};
	public:
		//you'd think this method name would be better in the parent class..
		//Returns FALSE when:
			/* file fails to download
			 * when we fail to update the node*/
		/*Returns TRUE when:
		 	* file is successfully downloaded & title=NULL or we update history */
		//what do we do when the file downloads successfully, but we fail to update the history? throw an exception & let the caller handle fixing the XML?
		bool download() {
			log.send("download()", logTRACE);
			if(!this->fetch()) return false;
			log.send("downloaded file", logTRACE);

			if (allocated_title == NULL) return true;
			log.send("allocated_title != NULL ==> updating history", logTRACE);
			log.send("allocated_title: " + std::string(allocated_title), logDEBUG);
			//text-nodes are nodes themself.... this the additional .first_node() call
			//https://stackoverflow.com/questions/62785421/c-rapidxml-traversing-with-first-node-to-modify-the-value-of-a-node-in-an
			associated_config.first_node("history")->first_node()->value(allocated_title);//config_document.allocate_string(entry_title.c_str()));
			log.send("Updated history", logTRACE);
			return true;
		};
	private:
		logger& log;
		const char* allocated_title;
		rx::xml_node<>& associated_config;
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
int main(void) {
	static logger &log = logger::getInstance(logWARNING);
	//signal handlers that ensure we properly deconstruct our static variables on exit
	std::signal(SIGSEGV, signal_handler);
	std::signal(SIGINT, signal_handler);
	//
	log.send("Starting RSS-Feed:");
	if (!fs::exists(CONFIG_NAME)) {
		std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
		log.send("CONFIG: " + std::string(CONFIG_NAME) + "does not exist", logERROR);
		log.send("Quitting...");
		exit(EXIT_FAILURE);
	}
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
	std::vector<rx::xml_node<char>*> config_nodes;
	log.send("Collect entries from the config", logTRACE);
	for (auto item_node = config_document.first_node()->first_node("item");
			item_node;
			item_node = item_node->next_sibling()) {
		log.send(" * Adding config node", logDEBUG);
		config_nodes.push_back(item_node);
	}
	//downloads in format of: NAME, URL
	std::vector<rssContents> download_links;
{//begin-scope
	if(!createFolderIfNotExist(RSS_FOLDER)) {
		log.send("Quitting...");
		exit(EXIT_FAILURE);
	}
	log.send("Checking Feeds:");
	for (auto config : config_nodes) {
	//1. download its linked RSS feed & set some data
		std::string configFeedName = config->first_node("feedFileName")->value();
		std::string url = config->first_node("feed-url")->first_node()->value();

		log.send("----------");
		log.send("NAME:" + configFeedName);
		log.send("URL:" + url, logDEBUG);

		download_entry config_download(url, RSS_FOLDER, configFeedName);
		if(!config_download.fetch()) {
			log.send("Failed to download file. SKIPPING...", logERROR);
			continue;
		}
		//
		std::string feedHistory = config->first_node("history")->value();
		log.send("HISTORY:" + feedHistory);
		//
	//2. parse the FEED
		rx::xml_document<> feed;
		rx::file<char> feedFile(config_download.getFileName());
		try {
			log.send("Parsing RSS", logTRACE);
			feed.parse<rx::parse_no_data_nodes>(feedFile.data());
		} catch(rx::parse_error &e) {
			log.send("Failed to parse" + std::string(e.what()) + "\nskipping...", logERROR);
			feed.clear();
			continue;
		}
	//3. Store download links & names to config
		bool first = true;
		char* regexpression = config->first_node("expr")->value();
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
				if (first) {
					download_links.push_back(rssContents(*config, config_document.allocate_string(entry_title.c_str()), entry_url, DOWNLOAD_FOLDER));
					first = false;
				}
				else download_links.push_back(rssContents(*config, NULL, entry_url, DOWNLOAD_FOLDER));
				log.send("Added " + entry_title + " to downloads");
			}
		}
		feed.clear();
	}
}//end-scope
	if (download_links.empty()) {
		log.send("No Downloads. Quitting...");
		exit(EXIT_SUCCESS);
	}
	log.send("<--" + std::to_string(download_links.size()) + " files to download-->");
	//download the files
	if(!createFolderIfNotExist(DOWNLOAD_FOLDER)) {
		log.send("Quitting...");
		exit(EXIT_FAILURE);
	}
	for (auto download : download_links) 
		download.download(); 
	//update the config
	log.send("Saving the updated config file", logTRACE);
	std::ofstream new_xml(CONFIG_NAME);
	new_xml << config_document;
	new_xml.close();
	log.send("Config file saved", logTRACE);
	config_document.clear();
	exit(EXIT_SUCCESS);
}
