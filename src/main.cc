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
//TODO: try using a c++ wrappper/binder for curl
//TODO: better quit functions & logging for what has been done
//TODO: store program data, such as the regular expressions & rss links, as well as download history in JSON/CSV files

//used with curl to download the file
static size_t write_data(char *ptr, size_t size, size_t nmemb, void *stream) {
	size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
	return written;
}
//downloads a file
bool downloadFile(std::string url, std::string fileName) {
		try {
			curlpp::Easy request;
			//set options, URL, Write callback function, progress output, &c
			request.setOpt<curlpp::options::Url>(url);
			request.setOpt<curlpp::options::NoProgress>(true);
			request.setOpt(curlpp::options::WriteFunctionCurlFunction(write_data));
			//write to the file
			FILE* pagefile = fopen(fileName.c_str(), "wb");
			if (pagefile) {
				request.setOpt<curlpp::OptionTrait<void*, CURLOPT_WRITEDATA>>(pagefile);
				request.perform();
			}
		}
		catch (curlpp::LogicError &e) {
			std::cout << e.what() << std::endl /*endl to flush the stream, might matter*/;
			return false;
		}
		catch (curlpp::RuntimeError &e) {
			std::cout << e.what() << std::endl;
			return false;
		}
		return true;
}
bool downloadFile(std::tuple<std::string, std::string> file) {
	return downloadFile(std::get<0>(file), std::get<1>(file));
}

//print the field of the node given by value
void print_by_value(rx::xml_node<> *node, std::string key) {
	std::cout << node->first_node(key.c_str())->value() << "\n";
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
 * 		<history>
 * 			<downloaded>title from feed of downloaded file</downloaded>
 * 			<downloaded>title from feed of downloaded file</downloaded>
 * 			... ad infinitum
 * 		</history>
 * 	</item>
 * 	...ad infinitum
 * </root>
 * */
class rssFeed {
	public:
		rssFeed(rx::xml_node<> *item_node) {
			this->title = item_node->first_node("title")->value();
			this->fileName = item_node->first_node("feedFileName")->value();
			this->url = item_node->first_node("feed-url")->first_node()->value();
			this->expression = item_node->first_node("expr")->value();
			//
			for (auto *history_node = item_node->first_node("history")->first_node();
					history_node;
					history_node = history_node->next_sibling()) {
				this->history.push_back(history_node->value());
			}
		};
		void getFeed() {
			std::cout << "getFeed()\n";
			rx::xml_document<> feed;
			//download the file
			if(!fs::exists(this->fileName))
				downloadFile(this->url, this->fileName);
			//loads the file
			if (!fs::exists(this->fileName)) throw std::runtime_error("file doesnt exist");

			rx::file<> file(this->fileName.c_str());
			try {
				feed.parse<rx::parse_declaration_node>(file.data());
			}
			catch (rx::parse_error &e) {
				std::cout << e.what() << std::endl;
				return;
			}
			//parse the file
			auto *channel = feed.first_node()->first_node(); //rss->channel
			for (auto *node = channel->first_node("item"); node; node = node->next_sibling()) {
				auto entry_title = node->first_node("title")->value();
				auto entry_url = node->first_node("link")->value();
				if (in_history(entry_title)){
					std::cout << "IN HISTORY!!!\n-----\n";
					break;
				}; //because it is in chronological order, we can just stop here
				if (match_title(this->expression, entry_title)) {
					downloads.push_back({entry_title, entry_url});
				}
			}
		}
		void print_info() {
			std::cout << ":"<< title << "\n";
			std::cout << ":"<< fileName << "\n";
			std::cout << ":"<< url << "\n";
			std::cout << ":"<< expression << "\n";
			std::cout << "[\n";
			for (auto entry : history) std::cout << entry << ",\n";
			std::cout << "]\n";
		}

		std::vector<std::tuple<std::string, std::string>> downloads; //title, url
	private:
		std::string title;
		std::string fileName;
		std::string url;
		std::string expression;
		std::vector<std::string> history; //only loaded at initialization. we will not be updating this
		//
		//checks if the given title is in the history
		bool in_history(std::string title) {
			for (auto entry : history)
				if (title == entry) return true;
			return false;
		}
};
#define CONFIG_NAME "rss-config.xml"
int main(int argc, char** argv) {
	{
		if (!fs::exists(CONFIG_NAME)) {
			std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
			exit(EXIT_FAILURE);
			//remove compiler warning for unused variable.
			(void)argv[1];
			(void)argc;
		}
		rx::xml_document<> config_document;
		rx::file<> config_file(CONFIG_NAME);
		config_document.parse<0>(config_file.data());
		for (auto item_node = config_document.first_node()->first_node();
				item_node;
				item_node = item_node->next_sibling()) {
			rssFeed feed(item_node);
			feed.print_info();
			feed.getFeed();
			for (auto download : feed.downloads) {
//				std::cout << std::get<0>(download) << " | " << std::get<1>(download) << "\n";
				//add the node to the history
				std::string element_name = "downloaded";
				char* node_name = config_document.allocate_string(element_name.c_str(), element_name.size());
				char* node_value = config_document.allocate_string(std::get<0>(download).c_str(), std::get<0>(download).size());
				rx::xml_node<> *new_node = config_document.allocate_node(rx::node_element,
						node_name, node_value
				);
				auto history_node = item_node->first_node("history");
				history_node->append_node(new_node);
			}
		}
		std::cout << "WRITING TO TESTING!!!\n";
		std::ofstream new_xml(CONFIG_NAME);
		new_xml << config_document;
		new_xml.close();
	}

	exit(EXIT_SUCCESS);
}
