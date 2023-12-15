#include <chrono>
#include <logger.hpp>
#include <download_handling.cpp>
#include <config.h>
//
//
#include <sys/select.h>
#include <thread>
#include <csignal> //reference for SIGINT & SIGSEGV
//
#include <rapidxml_print.hpp>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
//
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <filesystem>
#include <future>
//TODO: set in config.h
#define CONFIG_NAME "rss-config.xml"
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
//TODO: modify the download class to only enforce on rssContents, not when we download other files(like feeds which must be clobbered)
//TODO: Work with/around rate-limits to 

namespace rssfeed {
/*-------------------- BEGIN RSS FEED NAMESPACE --------------------*/
namespace fs = std::filesystem;
namespace rx = rapidxml;
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
void inititalize_program() {
	(void)write_data;
	static logger &log = logger::getInstance(logTRACE);
	log.send("starting initialization", logTRACE);
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
	std::signal(SIGSEGV, rssfeed::signal_handler);
	std::signal(SIGINT, signal_handler);
	log.send("initialization over", logTRACE);
}

int main(void) {
	inititalize_program();
	static logger &log = logger::getInstance(logWARNING);
	static download_manager &downloadManager = download_manager::getInstance();
	//config_document & config_file must live & die with the program. Consider placing them into a static class or struct, we rely on them being static such that they are cleaned up with exit(...)
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
			downloadManager.add(feeds.back());
			log.send("Added feed() to vector & download manager", logTRACE);
		} catch(std::runtime_error &e) {
			log.send(e.what(), logERROR);
		}
	}
	//download the feeds channel
	downloadManager.multirun();
	std::vector<std::future<std::vector<download_base>>> parsing_feeds;
	//parse each feed in a thread
	for (auto& current_feed : feeds) {
		log.send("creating thread",logTRACE);
		std::future<std::vector<download_base>> t = std::async(std::launch::async, &feed::parse,std::ref(current_feed));
		parsing_feeds.push_back(std::move(t));
	}
	//join threads
	log.send("joining threads");
	while (!parsing_feeds.empty()) {
		log.send(">checking feeds(" + std::to_string(parsing_feeds.size()) + ")");
		for (auto iterator = parsing_feeds.begin(); iterator != parsing_feeds.end(); iterator++) {
			auto future_feed = iterator;
			//if the future value is ready (should mean thread is done.)
			try {
				if (future_feed->wait_for(std::chrono::milliseconds(200)) == std::future_status::ready) {
					for (auto& val : future_feed->get()) {
						log.send("added download manager");
						downloadManager.add(val);
					}
					if (iterator == parsing_feeds.end()) {
						parsing_feeds.erase(iterator);
						break;
					} else {
						auto old_iterator = iterator;
						iterator -=1;
						parsing_feeds.erase(old_iterator);
					}
					log.send("REMOVED FUTURE");
				}
			}
			catch(std::future_error& e) {
				log.send("future_error:" + std::string(e.what()), logERROR);
				log.send("future_feed == valid? :" + std::string((future_feed->valid())?"true":"false"),logERROR);
					if (iterator == parsing_feeds.end()) {
						parsing_feeds.erase(iterator);
						break;
					} else {
						auto old_iterator = iterator;
						iterator -=1;
						parsing_feeds.erase(old_iterator);
					}
					log.send("REMOVED FUTURE - WRONGLY (should not have entered this state...)", logWARNING);
			}
		}
	}
	log.send("NO MORE FEEDS");
	//download the files in baches
	downloadManager.multirun();
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
