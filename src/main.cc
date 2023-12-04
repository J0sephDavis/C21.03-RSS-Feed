#include <cstdlib>
#include <tuple>
#include <iostream>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include <rapidxml_print.hpp>
#include "regex.c"
#include "curl/curl.h"
#include "curl/easy.h"
namespace rx = rapidxml;

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
//
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
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
	std::cout << "DOWNLOADS:\n";
	for (auto link : download_links) {
		std::cout << link << "\n";
	}
	CURL *curl_handle;
	curl_global_init(CURL_GLOBAL_ALL);
	for (auto link : download_links) {
		std::string file_name(link);
		auto iterator = file_name.find('/');
		for (;iterator != std::string::npos; iterator = file_name.find('/'))
			file_name.replace(iterator, 1, "");
		//taken from a curl example on their webpage
		curl_handle = curl_easy_init();
		FILE *pagefile;
		curl_easy_setopt(curl_handle, CURLOPT_URL, link.c_str());
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
		pagefile = fopen(file_name.c_str(), "wb");
		if (pagefile) {
			std::cout << "good file";
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
			curl_easy_perform(curl_handle);
			fclose(pagefile);
		}
		curl_easy_cleanup(curl_handle);
	}
	curl_global_cleanup();
	//
	exit(EXIT_SUCCESS);
}
