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
namespace rx = rapidxml;
//TODO: have a list of regular expressions and an RSS feed that is tailored to only find similar files to that one
//Then, record the last downloaded file & download if they differ in titles
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
int main(int argc, char** argv) {
	if (argc < 2) {
		std::cout << "Missing filepath\n";
		exit(EXIT_FAILURE);
	}
	std::vector<std::tuple<std::string, bool>> titles = {
		{"Toaru Ossan", false},
		{"Boushoku no Berserk", false}
	};

	rx::xml_document<> doc;
	rx::file<> xml_file(argv[1]);
	doc.parse<0>(xml_file.data());
	//
	std::vector<std::string> download_links;
	//
	auto *item_parent = doc.first_node()->first_node();
	for (auto *node = item_parent->first_node("item"); node; node = node->next_sibling()) {
		for (auto &title : titles) {
			if(std::get<1>(title)) continue;
			if (match_title(std::get<0>(title), node->first_node("title")->value())){
				//
				print_by_value(node, "title");
				//
				std::get<1>(title) = 1;
				download_links.push_back(node->first_node("link")->value());
				break;
			};
		}
	}

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
