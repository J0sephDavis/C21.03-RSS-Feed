#ifndef RSS_LOGGER
#define RSS_LOGGER
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <mutex>
#include <queue>
#include <config.h>
//

namespace rssfeed {
//begin namespace
namespace fs = std::filesystem;
enum logLevel_t {
	logDEBUG, 	//
	logTRACE, 	//trace exeuction across project. What logic was done
	logWARN, 	//Things that might have caused problems but don't affect the end functionality of the system
	logINFO, 	//In gist, what has been/is being done
	logERROR 	//a real problem that is detrimental to the functionality of the system.
};
class logger {
	//https://stackoverflow.com/questions/1008019/how-do-you-implement-the-singleton-design-pattern
	//Modified from: https://drdobbs.com/cpp/logging-in-c/201804215
	//TODO: have log file update during program runtime, not just when it exits successfully...
	public:
		static logger& getInstance(logLevel_t level = logINFO) {
			//https://stackoverflow.com/questions/335369/finding-c-static-initialization-order-problems/335746#335746
			//Take a look at destruction problems in that thread
			static logger instance(level);
			instance.send("logger::getInstance", logTRACE);
			return instance;
		}
		 logger(logger const &) = delete;
		 void operator=(logger const &) = delete;
		~logger() {
			send("logger::~logger", logTRACE);
			os << std::endl; //flush
			std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			const std::string log_prefix = "rss-feed-";
			const std::string log_suffix = ".log";
#define timestamp_len 16
			std::string datetime(timestamp_len,0);
			//https://stackoverflow.com/questions/28977585/how-to-get-put-time-into-a-variable
			datetime.resize(std::strftime(&datetime[0], datetime.size(), "%Y-%m-%d_%H-%M", std::localtime(&time)));
			std::string fileName = LOG_FOLDER + log_prefix + datetime + log_suffix;
			if(!fs::exists(LOG_FOLDER)) {
				//we don't catch the error, let it fail...
				fs::create_directory(LOG_FOLDER);
			}
			//
			std::cout << "log file=" << fileName << std::endl;
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
		inline void trace(std::string message) {
			send(std::move(message), logTRACE);
		}
		inline void debug(std::string message) {
			send(std::move(message), logDEBUG);
		}
		inline void info(std::string message) {
			send(std::move(message), logINFO);
		}
		inline void warn(std::string message) {
			send(std::move(message), logWARN);
		}
		inline void error(std::string message) {
			send(std::move(message), logERROR);
		}
	private:
		logger(logLevel_t level):
			loggerLevel(level) {
				send("logger::logger", logTRACE);
				send("VERSION" + std::string(PROJECT_VERSION));
			};
	private:
		std::string levelString(logLevel_t level) {
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
		void clear_queue() {
			std::lock_guard lock(queue_write);
			for (; !messages.empty(); messages.pop()) {
				auto& msg = messages.front();
				os << msg.str();
			}
		}
		//sends a message to the log
		void send(std::string message, logLevel_t level = logINFO) {
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
	private:
		logLevel_t loggerLevel;
		//the actual output stream
		std::ostringstream os;
		std::queue<std::ostringstream>  messages;
		std::mutex queue_write;
};
static logger& log = logger::getInstance(logTRACE);
//end namespace
}
#endif
