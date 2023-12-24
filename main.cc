#include <chrono>
#include <logger.cpp>
#include <download_handling.cpp>
#include <config.h>
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
 * 		<expr>regex here</expre>
 * 		<history>title of the last downloaded rss entry</history>
 * 	</item>
 * 	...ad infinitum
 * </root>
 * */

//TODO: make an easy way for the user to update the config
//TODO: maintain a list of files that failed to download. Try to download them again before logging them and quitting
//TODO: remove regex? might not be needed if the RSS is filtered right
//TODO: modify the download class to only enforce on rssContents, not when we download other files(like feeds which must be clobbered)

namespace rssfeed {
/*-------------------- BEGIN RSS FEED NAMESPACE --------------------*/
namespace fs = std::filesystem;
namespace rx = rapidxml;
//checks if a folder exists, if not it attempts to create the folder. returns true on success
bool createFolderIfNotExist(fs::path folder) {
	if(!fs::exists(folder)) {
		log.warn(folder.string() + " does not exist. Attempting to create.");
		try {
			fs::create_directory(folder);
			log.info("Created folder");
		} catch (fs::filesystem_error &e) {
			log.error("Failed to create folder(" + folder.string() + ")\n" + e.what());
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
	log.trace("initializing");
	if (!fs::exists(CONFIG_NAME)) {
		std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
		log.error("CONFIG: " + std::string(CONFIG_NAME) + "does not exist");
		log.info("Quitting...");
		exit(EXIT_FAILURE);
	}
	if(!createFolderIfNotExist(RSS_FOLDER)) {
		log.info("Quitting...");
		exit(EXIT_FAILURE);
	}
	if(!createFolderIfNotExist(DOWNLOAD_FOLDER)) {
		log.info("Quitting...");
		exit(EXIT_FAILURE);
	}
	//signal handlers that ensure we properly deconstruct our static variables on exit
	std::signal(SIGSEGV, rssfeed::signal_handler);
	std::signal(SIGINT, signal_handler);
	log.trace("initialized");
}

int main(void) {
	inititalize_program();
	static download_manager &downloadManager = download_manager::getInstance();
	//config_document & config_file must live & die with the program. Consider placing them into a static class or struct, we rely on them being static such that they are cleaned up with exit(...)
	static rx::xml_document<> config_document;
	static rx::file<> config_file(CONFIG_NAME);
	try {
		log.trace("Parsing config document");
		config_document.parse<0>(config_file.data());
	}
	catch (rx::parse_error &e) {
		log.error("Failed to parse config:" + std::string(e.what()));
		exit(EXIT_FAILURE);
	}

	//pointers to each config node
	std::vector<feed> feeds;
	log.trace("Collect entries from the config");
	for (auto item_node = config_document.first_node()->first_node("item");
			item_node;
			item_node = item_node->next_sibling()) {
		std::string configFeedName = item_node->first_node("feedFileName")->value();
		std::string url = item_node->first_node("feed-url")->first_node()->value();
		std::string feedHistory = item_node->first_node("history")->value();
		char* regexpression = item_node->first_node("expr")->value();

		auto tmp = feed(*item_node, configFeedName, url, regexpression, feedHistory);
		feeds.emplace_back(std::move(tmp));
		downloadManager.add(feeds.back());
	}
	//download the feeds channel
	downloadManager.multirun();
	std::vector<std::future<std::vector<download_base>>> parsing_feeds;
	//parse each feed in a thread
	log.trace("split into threads");
	for (auto& current_feed : feeds) {
		std::future<std::vector<download_base>> t = std::async(std::launch::async, &feed::parse,std::ref(current_feed));
		parsing_feeds.push_back(std::move(t));
	}
	//join threads
	while (!parsing_feeds.empty()) {
		log.trace("waiting for " + std::to_string(parsing_feeds.size()) + " threads to return");
		for (auto iterator = parsing_feeds.begin(); iterator != parsing_feeds.end(); iterator++) {
			auto future_feed = iterator;
			//if the future value is ready (should mean thread is done.)
			try {
				if (future_feed->wait_for(std::chrono::milliseconds(200)) == std::future_status::ready) {
					log.trace("receiving future response");
					for (auto& val : future_feed->get()) {
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
					log.trace("removed future");
				}
			}
			catch(std::future_error& e) {
				log.error("future_error:" + std::string(e.what()));
				log.error("future_feed == valid? :" + std::string((future_feed->valid())?"true":"false"));
					if (iterator == parsing_feeds.end()) {
						parsing_feeds.erase(iterator);
						break;
					} else {
						auto old_iterator = iterator;
						iterator -=1;
						parsing_feeds.erase(old_iterator);
					}
				log.warn("REMOVED FUTURE - WRONGLY (should not have entered this state...)");
			}
		}
	}
	log.trace("threads returned");
	//download the files in baches
	downloadManager.multirun();
	log.trace("check for updated histories");
	for (auto& current_feed : feeds) {
		if (current_feed.isNewHistory()) {
			current_feed.getConfigRef().first_node("history")->first_node()->value(config_document.allocate_string(current_feed.getHistory().c_str()));
			log.trace("Updated history");
			log.debug("newHistory:" + current_feed.getHistory());
		}
	}
	
	//update the config
	log.trace("Saving the updated config file");
	std::ofstream new_xml(CONFIG_NAME);
	new_xml << config_document;
	new_xml.close();
	log.trace("Config file saved");
	config_document.clear();
	exit(EXIT_SUCCESS);
}
