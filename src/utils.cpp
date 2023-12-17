#include <logger.hpp>
#include <regex.c>
namespace rssfeed{
static size_t write_data(char *ptr, size_t size, size_t nmemb, void *stream) {
	size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
	return written;
}
static void signal_handler(int signal) {
	log.error("Signal(" + std::to_string(signal) + ") raised. quitting...");
	std::cout << "signal(" << signal << ") received: quitting\n";
	exit(EXIT_FAILURE);
}
//returns whether the given title matches the expression
bool match_title(std::string Regular_Expression, std::string title) {
	log.trace("(rssfeed)match_title");
	log.debug("expression: "  + Regular_Expression +"\ttitle:" + title);
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
