#include <cstdlib>
#include <curlpp/Exception.hpp>
#include <curlpp/OptionBase.hpp>
#include <stdexcept>
#include <tuple>
#include <iostream>
#include <rapidxml_print.hpp>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include "regex.c"
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <filesystem>

namespace rx = rapidxml;
namespace fs = std::filesystem;

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
bool downloadFile(std::string url, std::string fileName) {
//TODO: handle fclose() & returns in a clean manner
	std::cout << "downloading: " << fileName << " (" << url << ")\n";
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
		std::cout << e.what() << std::endl /*endl to flush the stream, might matter*/;
	}
	catch (curlpp::RuntimeError &e) {
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
#define CONFIG_NAME "rss-config.xml"
int main(void) {
	{
		//NAME, URL
		std::vector<std::tuple<std::string, std::string>> download_links;
		if (!fs::exists(CONFIG_NAME)) {
			std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
			exit(EXIT_FAILURE);
		}
		rx::xml_document<> config_document;
		rx::file<> config_file(CONFIG_NAME);
		try {
			config_document.parse<0>(config_file.data());
		}
		catch (rx::parse_error &e) {
			std::cout << e.what() << std::endl;
			exit(EXIT_FAILURE);
		}
		//pointers to each config node
		std::vector<rx::xml_node<char>*> config_nodes;
		for (auto item_node = config_document.first_node()->first_node();
				item_node;
				item_node = item_node->next_sibling()) {
			config_nodes.push_back(item_node);
		}
		for (auto config : config_nodes) {
			std::cout << "----------\n";
		//1. download its linked RSS feed & set some data
			std::string configFeedName = "rss_feeds/";
			configFeedName += config->first_node("feedFileName")->value();
			std::string url = config->first_node("feed-url")->first_node()->value();
			if(!downloadFile(url, configFeedName)) {
				std::cout << "could not download the file. skipping...\n";
				continue;
			}
			rx::file<char> feedFile(configFeedName);
			std::string feedHistory = config->first_node("history")->value();
		//2. parse the FEED
			std::cout << "FEED:" << configFeedName << "\n";
			rx::xml_document<> feed;
			try {
				feed.parse<rx::parse_no_data_nodes>(feedFile.data());
			} catch(rx::parse_error &e) {
				std::cout << "parse error:" << e.what() << "\n";
				feed.clear();
				std::cout << "skipping...\n";
				continue;
			}
		//3. Store download links & names to config
			char* regexpression = config->first_node("expr")->value();
			if (!feedHistory.empty())
				std::cout << "history:" << feedHistory << "\n";
			std::string newHistory;
			for (auto *node = feed.first_node()->first_node()->first_node("item");
					node;
					node = node->next_sibling()) {
				std::string entry_title = node->first_node("title")->value();
				std::string entry_url = node->first_node("link")->value();
				if (entry_title.compare(feedHistory) == 0)
					break; //because it is in chronological order, we can just stop here
				if (newHistory.empty())
					newHistory = entry_title;
				if (match_title(regexpression, entry_title)) {
					download_links.push_back({entry_title, entry_url});
				}
			}
			//TODO make sure this is the correct way to allocate a string to replace an old value
			if (!newHistory.empty())
				config->first_node("history")->value(config_document.allocate_string(newHistory.c_str()));
			feed.clear();
		}
		std::cout << "---DOWNLOADS(" << download_links.size() << ")---\n";
		//download the files
		for (auto download : download_links) {
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
		//download
			std::string fileName = "downloads/" + std::get<0>(download);
			if (!downloadFile(std::get<1>(download), fileName)) {
				std::cout << "failed to download:" + fileName;
				continue;
			}
		}
		//update the config
		std::ofstream new_xml(CONFIG_NAME);
		new_xml << config_document;
		new_xml.close();
		config_document.clear();
	}
	exit(EXIT_SUCCESS);
}
