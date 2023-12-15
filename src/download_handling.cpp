#include <download_handling.hpp>
namespace rssfeed {
//download_base START
download_base::download_base(std::string url,fs::path download_path) :
	log(logger::getInstance()),
	url(url)
{
	filePath = download_path;
	log.send("download_base::download_base", logTRACE);
	log.send("called with:\n\turl:" + url + "\n\tdownload_path:" + download_path.string(), logDEBUG);
}
download_base::~download_base() {
	log.send("download_base::~download_base",logTRACE);
}
//downloads a file
bool download_base::fetch() {
	log.send("download_base::fetch", logTRACE);
	log.send("url:" + url + "\tpath: " + filePath.string(), logDEBUG);
	if(fs::exists(filePath)) {
		log.send("FILE ALREADY EXISTS", logWARNING);
#if !CLOBBER_FLAG
		return false;
#endif
	}
	//TODO: handle fclose() & returns in a clean manner
	//this section of code somewhat irks me. Not the curlpp,
	//but the error handling & that we are using a FILE* rather than an fstream.
	//Must look into how we can get to an fstream with curlpp write callback
	FILE* pagefile = fopen(filePath.c_str(), "wb");
	try {
		//set options, URL, Write callback function, progress output, &c
		curlpp::Easy request;
		request.setOpt<curlpp::options::Url>(url);
		request.setOpt<curlpp::options::NoProgress>(true);
		request.setOpt(curlpp::options::WriteFunctionCurlFunction(rssfeed::write_data));
		//write to the file
		if (pagefile) {
			request.setOpt<curlpp::OptionTrait<void*, CURLOPT_WRITEDATA>>(pagefile);
			request.perform();
			fclose(pagefile);
			log.send("File downloaded successfully");
			return true;
		}
		else {
			perror("Error");
			return false;
		}
	}
	catch (curlpp::LogicError &e) {
		log.send("Failed to download file: " + std::string(e.what()), logERROR);
	}
	catch (curlpp::RuntimeError &e) {
		log.send("Failed to download file: " + std::string(e.what()), logERROR);
	}
	fclose(pagefile);
	return false;
}
const fs::path download_base::getPath() {
	return filePath;
}
const std::string download_base::getURL() {
	return url;
}
//download_base END

//download_manager START
download_manager& download_manager::getInstance()
{
	static download_manager instance;
	return instance;
}
download_manager::~download_manager() {
	log.send("download_manager::~download_manager", logTRACE);
}
download_manager::download_manager():
	log(logger::getInstance())
{
	log.send("download_manager::download_manager", logTRACE);
	curlpp::initialize();
}
//adds download by referece, current intent is for use with feeds
void download_manager::add(download_base& download) {
	log.send("download_manager::add",logTRACE);
	//TODO determine if this avoids copy-construction
	std::lock_guard<std::mutex> lock(queue_lock);
	downloads.emplace(download);
}
std::vector<std::pair<curlpp::Easy *, FILE *>> download_manager::getRequests(size_t num_get) {
	std::vector<std::pair<curlpp::Easy *, FILE *>> requests;
	log.send("download_manager::getRequests",logTRACE);
	//prevent the queue from being touched
	std::lock_guard lock(queue_lock);
	for (size_t i = 0; i < num_get && !downloads.empty(); i++) {
		curlpp::Easy *request = new curlpp::Easy();
		FILE* pagefile = fopen(downloads.front().getPath().c_str(), "wb");
		if (pagefile) {
			request->setOpt(new curlpp::options::WriteFile(pagefile));
			request->setOpt(new curlpp::Options::Url(downloads.front().getURL()));
			request->setOpt(new curlpp::Options::NoProgress(true));
			request->setOpt(new curlpp::Options::WriteFunctionCurlFunction(write_data));

			requests.push_back(std::pair(std::move(request),std::move(pagefile)));
			downloads.pop();
		}
		else {
			log.send("FAILED TO ADD REQUEST BECAUSE PAGEFILE=NULL",logERROR);
			break;
		}
	}
	return (requests);
}
void download_manager::multirun() {
	const int batchSize = 4;
	log.send("download_manager::multirun", logTRACE);
	log.send("batch size: " + std::to_string(batchSize), logDEBUG);
/*
 * Current problem: multi_perform makes too many requests too quickly causes for the host to rate-limit us & return bad-data.
 * However, if we use sequential downloads of easy_requests, they take far too long going one-by-one.
 * Solution: Instead of operating on the set N of requests, split into subsets to gather in batches.
 * If N=20 request, perform in small batches of approximately n=4 with a sleep timer between.
 * */
	while(!downloads.empty()) {
		log.send("Begin downloading batch", logTRACE);
		CURLM *multi_handle = curl_multi_init();
		long max_conn = (batchSize > 2) ? batchSize/2 : 2;
		log.send("max_host_connections = " + std::to_string(max_conn),logDEBUG);
		curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, max_conn);
		//get the requests & tie them to files
		auto requests = getRequests(batchSize);
		if (requests.empty()) {
			log.send("NO REQUESTS", logERROR);
			break;
		}
		for (auto& request : requests) {
			curl_multi_add_handle(multi_handle, request.first->getHandle());
			log.send("Added easy handle to multi_hanle", logDEBUG);
		}
		//https://github.com/jpbarrette/curlpp/blob/master/examples/example13.cpp
		int handles_left;
		do {
			log.send("multi_perform loop:", logDEBUG);
			CURLMcode multi_code = curl_multi_perform(multi_handle, &handles_left);
			log.send("handles_left:" + std::to_string(handles_left) ,logDEBUG);
			if (!multi_code && handles_left) {
				//wait for activity or timeout or "nothing" -> https://curl.se/libcurl/c/curl_multi_perform.html
				multi_code = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);
			}
			if (multi_code) {
				log.send("curl_multi_poll() failed with code:" + std::to_string(multi_code),logERROR);
				break;
			}
			//there are more transfers, LOOP
		} while(handles_left);
		CURLMsg* multi_info;
		do {
			log.send("checking for multi_info messages", logTRACE);
			multi_info = curl_multi_info_read(multi_handle,&handles_left);
			if(multi_info) {
				if (multi_info->msg == CURLMSG_DONE) {
					log.send("multi_info->msg = CURLMSG_DONE", logDEBUG);
					for (auto handle : requests) {
						if (handle.first->getHandle() == multi_info->easy_handle) {
							log.send("removing handles from multi_handle & closing files", logTRACE);
							log.send("handle result:" + std::to_string(multi_info->data.result), logDEBUG);
							fclose(handle.second);
							curl_multi_remove_handle(multi_handle, multi_info->easy_handle);
						}
					}
				}
				else log.send("multi_info->msg = " + std::to_string(multi_info->msg));
			}
			else log.send("multi_info == NULL", logDEBUG);
		} while(multi_info);
		curl_multi_cleanup(multi_handle);
		log.send("cleanup multi handle", logTRACE);
		log.send("SLEEP TWO SECONDS", logDEBUG);
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
}
//download_manager END
//feed START
namespace rx = rapidxml;
feed::feed(rx::xml_node<>& config_ptr, std::string fileName, std::string url, char* regex, std::string history):
	download_base(url, RSS_FOLDER + fileName),
	config_ref(config_ptr),
	feedHistory(history),
	regexpression(regex)
{
	log.send("feed::feed()",logTRACE);
	log.send("NAME:" + this->getPath().string());
	log.send("URL:" + url, logDEBUG);
	log.send("HISTORY:" + feedHistory);
}
//only call if it has been downloaded
std::vector<download_base> feed::parse() {
	std::vector<download_base> rv_downloads;
	log.send("feed::parse", logTRACE);
	if (doneParsing) return {};
//2. parse the FEED
	rx::xml_document<> feed;
	rx::file<char> feedFile(getPath());
	try {
		log.send("Parsing RSS", logTRACE);
		feed.parse<rx::parse_no_data_nodes>(feedFile.data());
	} catch(rx::parse_error &e) {
		log.send("Failed to parse" + std::string(e.what()) + "\nskipping...", logERROR);
		feed.clear();
		throw std::runtime_error("Failed to parse file");
	}
//3. Store download links & names to config
	log.send("Regular Expression:" + std::string(regexpression), logDEBUG);
	log.send("Begin fetching downloads", logTRACE);
	for (auto *node = feed.first_node()->first_node()->first_node("item");
			node;
			node = node->next_sibling()) {
		std::string entry_title = node->first_node("title")->value();
		log.send("ENTRY TITLE: " + entry_title, logDEBUG);
		std::string entry_url = node->first_node("link")->value();
		log.send("URL: " + entry_url, logDEBUG);
		if (entry_title.compare(feedHistory) == 0) {
			log.send("Download in history. skipping...", logTRACE);
			break; //because it is in chronological order, we can just stop here
		}
		if (match_title(regexpression, entry_title)) {
			if (newHistory == false) {
				newHistory = true;
				newHistoryTitle = entry_title;
				log.send("set new history", logTRACE);
			}
			auto tmp_file = download_base(entry_url, fs::path(DOWNLOAD_FOLDER + url_to_filename(entry_url)));
			rv_downloads.push_back(std::move(tmp_file));
			log.send("Added " + entry_title + " to downloads");
		}
	}
	feed.clear();
	doneParsing = true;
	return rv_downloads;
}
//returns the pointer to the xml child in the config relating to this feed
const rx::xml_node<>& feed::getConfigRef() {
	log.send("feed::getConfigRef", logTRACE);
	return config_ref;
}
//returns true if a file is slated for download
bool feed::isNewHistory() {
	log.send("feed::isNewHistory",logTRACE);
	return newHistory;
}
//if downloads != empty, then the history received MUST be new
const std::string feed::getHistory() {
	log.send("feed::getHistory", logTRACE);
	if (newHistory) return newHistoryTitle;
	else return feedHistory;
}
//feed END
//END NAMESAPCE
}
