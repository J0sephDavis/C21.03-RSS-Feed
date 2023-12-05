#include <cstdlib>
#include <curlpp/Exception.hpp>
#include <curlpp/OptionBase.hpp>
#include <tuple>
#include <iostream>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include <rapidxml_print.hpp>
#include "regex.c"
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <filesystem>
namespace rx = rapidxml;
//TODO: try using a c++ wrappper/binder for curl
//TODO: better quit functions & logging for what has been done
//TODO: store program data, such as the regular expressions & rss links, as well as download history in JSON/CSV files


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
//used with curl to download the file
static size_t write_data(char *ptr, size_t size, size_t nmemb, void *stream) {
	size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
	return written;
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
			item_node = item_node->first_node("history");
			for (auto *history_node = item_node->first_node();
					history_node;
					history_node = history_node->next_sibling()) {
				this->history.push_back(history_node->value());
			}
		};
		bool in_history(std::string title) {
			for (auto entry : history)
				if (title == entry) return true;
			return false;
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
	private:
		std::string title;
		std::string fileName;
		std::string url;
		std::string expression;
		std::vector<std::string> history;
};
#define CONFIG_NAME "rss-config.xml"
namespace fs = std::filesystem;
int main(int argc, char** argv) {
	{
		if (!fs::exists(CONFIG_NAME)) {
			std::cout << "PLEASE POPULATE: " << CONFIG_NAME << "\n";
			exit(EXIT_FAILURE);
			std::cout << argv[1]; //remove compiler warning for unused variable. probably should make the compilation less strict
		}
		rx::xml_document<> config_document;
		rx::file<> config_file(CONFIG_NAME);
		config_document.parse<0>(config_file.data());
		auto item_node = config_document.first_node()->first_node();
		rssFeed blah(item_node);
		blah.print_info();
	}

	exit(EXIT_SUCCESS);
//--------------------------------------------------
	if (argc < 2) {
		std::cout << "Missing filepath\n";
		exit(EXIT_FAILURE);
	}
//	std::vector<std::tuple<std::string, bool>> titles = {
//		{"Toaru Ossan", false},
//		{"Boushoku no Berserk", false}
//	};
//
//	//
	std::vector<std::string> download_links; //the links of the files to download
//	//
//	{ //
//		rx::xml_document<> doc;
//		rx::file<> xml_file(argv[1]);
//		doc.parse<0>(xml_file.data());
//		auto *item_parent = doc.first_node()->first_node();
//		for (auto *node = item_parent->first_node("item"); node; node = node->next_sibling()) {
//			for (auto &title : titles) {
//				if(std::get<1>(title)) continue;
//				if (match_title(std::get<0>(title), node->first_node("title")->value())){
//					//
//					print_by_value(node, "title");
//					//
//					std::get<1>(title) = 1;
//					download_links.push_back(node->first_node("link")->value());
//					break;
//				};
//			}
//		}
//	}

	//downloads
	//curlpp::Cleaner is not needed, obsolete according to the header files
//	curl_global_init(CURL_GLOBAL_ALL); //TODO
	for (auto link : download_links) {
		std::cout << "DOWNLOADING:" << link << "\n";
		std::string file_name(link);
		auto iterator = file_name.find('/');
		//clean the name of any slashes
		for (;iterator != std::string::npos; iterator = file_name.find('/'))
			file_name.replace(iterator, 1, "");
		//-----
		try {
			curlpp::Easy request;
			//set options, URL, Write callback function, progress output, &c
			request.setOpt<curlpp::options::Url>(link);
			request.setOpt<curlpp::options::NoProgress>(true);
			request.setOpt(curlpp::options::WriteFunctionCurlFunction(write_data));
			//write to the file
			FILE* pagefile = fopen(file_name.c_str(), "wb");
			if (pagefile) {
				request.setOpt<curlpp::OptionTrait<void*, CURLOPT_WRITEDATA>>(pagefile);
				request.perform();
			}
		}
		catch (curlpp::LogicError &e) {
			std::cout << e.what() << std::endl /*endl to flush the stream, might matter*/;
			exit(EXIT_FAILURE);
		}
		catch (curlpp::RuntimeError &e) {
			std::cout << e.what() << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	exit(EXIT_SUCCESS);
}
