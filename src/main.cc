#include <logger.hpp>
#include <download_handling.hpp>
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
			downloadManager.add(url, tmp.getPath());
			feeds.emplace_back(std::move(tmp));
			log.send("Added feed() to vector & download manager", logTRACE);
		} catch(std::runtime_error &e) {
			log.send(e.what(), logERROR);
		}
	}
	//download the feeds channel
	downloadManager.multirun(4);
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
	//download the files in baches
	downloadManager.multirun(4);
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
