#include <logger.hpp>
#include <C1402_regex.cc>
namespace rssfeed{
	using namespace C1402_regex;
static size_t write_data(char *ptr, size_t size, size_t nmemb, void *stream) {
	size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
	return written;
}
static void signal_handler(int signal) {
	log.error("Signal(" + std::to_string(signal) + ") raised. quitting...");
	std::cout << "signal(" << signal << ") received: quitting\n";
	exit(EXIT_FAILURE);
}
bool match(regex* expression, std::string input_text) {
	char* text = (char*)calloc(1,input_text.size());
	char* start_of_text = text;
	strncpy(text, input_text.c_str(), input_text.size());
	char* end_of_match = NULL;
	do {
		end_of_match = expression->match_here(text);
		if (end_of_match != NULL)
			break;
	} while(*text++ != '\0');
	if (end_of_match != NULL) {
//		std::cout << "found match of length:" << (end_of_match - text) << "\n";
//		std::cout << "match begins after " << (text - start_of_text) << " chars\n";
//		char* matched_text = (char*)calloc(1, end_of_match - text);
//		strncpy(matched_text, text, end_of_match-text);
//		std::cout << "matched text: [" << std::string(matched_text) << "]\n";
		return true;
	}
	return false; 
	free(start_of_text);
}
//returns whether the given title matches the expression
bool match_title(std::string Regular_Expression, std::string title) {
	log.trace("(rssfeed)match_title");
	log.debug("expression: "  + Regular_Expression +"\ttitle:" + title);
	regex* expression = utils::create_from_string(Regular_Expression);
	bool retval = match(expression, title);
	//this is to destroy the expression... I really ought to edit C14.02 and move this into a function there
	log.debug("match_title return:" + std::string((retval)?"true":"false"));
	return retval;
}

std::string url_to_filename(const std::string url) {
	log.trace("(rssfeed)url_to_filename");
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
}
