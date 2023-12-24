#ifndef RSS_LOGGER_IMPLEMENT
#define RSS_LOGGER_IMPLEMENT
#include <logger.hpp>

namespace rssfeed {
	namespace fs = std::filesystem;
logger& logger::getInstance(logLevel_t level) {
	//https://stackoverflow.com/questions/335369/finding-c-static-initialization-order-problems/335746#335746
	//Take a look at destruction problems in that thread
	static logger instance(level);
	instance.send("logger::getInstance", logTRACE);
	return instance;
}
logger::~logger() {
	send("logger::~logger", logTRACE);
	os << std::endl; //flush
	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	const std::string log_prefix = "rss-feed-";
	const std::string log_suffix = ".log";
	//
	std::stringstream datetime;
	datetime << std::put_time(std::localtime(&time), "%Y-%m-%d_%H-%M");
	//
	std::string fileName = LOG_FOLDER + log_prefix + datetime.str() + log_suffix;
	if(!fs::exists(LOG_FOLDER)) {
		//we don't catch the error, let it fail...
		fs::create_directory(LOG_FOLDER);
	}
	//
	std::cout << "\nlog file=" << fileName << std::endl;
	std::ofstream logFile(fileName);
	clear_queue();
	logFile << os.str();
	logFile.close();
}
/*
 * If you want to disable any of these log levels, such that they do not get compiled.
 * Replace the send(...) with a (void)message. Then the compiler should understand
 * that this code is pointless, whereupon it will ignore it.
 * */
inline void logger::trace(std::string message) {
	send(std::move(message), logTRACE);
}
inline void logger::debug(std::string message) {
	send(std::move(message), logDEBUG);
}
inline void logger::info(std::string message) {
	send(std::move(message), logINFO);
}
inline void logger::warn(std::string message) {
	send(std::move(message), logWARN);
}
inline void logger::error(std::string message) {
	send(std::move(message), logERROR);
}
logger::logger(logLevel_t level):
	loggerLevel(level)
{
	send("logger::logger", logTRACE);
	send("VERSION" + std::string(PROJECT_VERSION));
};
std::string logger::levelString(logLevel_t level) {
	if (level == logTRACE)
		return "TRACE";
	if (level == logDEBUG)
		return "DEBUG";
	if (level == logINFO)
		return "INFO";
	if (level == logWARN)
		return "WARN";
	if (level == logERROR)
		return "ERROR";
	return "unknown-loglevel";
}
void logger::clear_queue() {
	std::lock_guard lock(queue_write);
	for (; !messages.empty(); messages.pop()) {
		auto& msg = messages.front();
		os << msg.str();
	}
}
//sends a message to the log
void logger::send(std::string message, logLevel_t level) {
	if (messages.size() > 10) clear_queue();
	//only print if we are >= to the logger level
	if (level >= loggerLevel) {
		std::lock_guard lock(queue_write); //released when function ends
		std::ostringstream tmp_output;
		std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		//each log will be "TIME LEVEL MESSAGE"
		tmp_output << "\n." << std::put_time(std::localtime(&time),"%H:%M:%S");
		tmp_output << "[" << levelString(level)  << "]:\t";
		tmp_output << message;
#ifdef COUT_LOG
		std::cout << tmp_output.str();
#endif
		messages.push(std::move(tmp_output));
	}
}
//end namespace
}
#endif
