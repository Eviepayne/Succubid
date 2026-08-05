#include "HandyAPIClient.hpp"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <algorithm>

using json = nlohmann::json;

long long HandyAPIClient::getCurrentTimeMillis() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   std::chrono::system_clock::now().time_since_epoch())
		   .count();
}

long long HandyAPIClient::getCurrentSteadyMillis() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   std::chrono::steady_clock::now().time_since_epoch())
		   .count();
}

size_t HandyAPIClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::vector<std::string> HandyAPIClient::findScripts(const std::string &videoPath) {
	std::vector<std::string> foundScripts;
	std::filesystem::path video(videoPath);

	std::filesystem::path parentDir = video.has_parent_path() ? video.parent_path() : ".";
	std::string videoStem = video.stem().string();
	std::string videoFilename = video.filename().string();

	if(!std::filesystem::exists(parentDir) || !std::filesystem::is_directory(parentDir)) {
		return foundScripts;
	}

	const std::unordered_set<std::string> validExtensions = {".funscript", ".json", ".csv"};

	for(const auto &entry : std::filesystem::directory_iterator(parentDir)) {
		if(!entry.is_regular_file())
			continue;

		const auto &candidatePath = entry.path();
		std::string ext = candidatePath.extension().string();

		if(validExtensions.find(ext) == validExtensions.end())
			continue;

		std::string candidateStem = candidatePath.stem().string();

		bool matchesNormal =
			candidateStem == videoStem ||
			(candidateStem.rfind(videoStem + " (", 0) == 0 && candidateStem.back() == ')');

		bool matchesWithExtension =
			candidateStem == videoFilename ||
			(candidateStem.rfind(videoFilename + " (", 0) == 0 && candidateStem.back() == ')');

		if(matchesNormal || matchesWithExtension) {
			foundScripts.push_back(candidatePath.string());
		}
	}

	auto getExtPriority = [](const std::string & ext) -> int {
		if(ext == ".funscript")
			return 0;
		if(ext == ".csv")
			return 1;
		if(ext == ".json")
			return 2;
		return 3;
	};

	auto getNamePriority = [&](const std::filesystem::path & path) -> int {
		std::string stem = path.stem().string();

		if(stem == videoStem)
			return 0;

		if(stem == videoFilename)
			return 1;

		if(stem.rfind(videoStem + " (", 0) == 0)
			return 2;

		if(stem.rfind(videoFilename + " (", 0) == 0)
			return 3;

		return 4;
	};

	std::sort(foundScripts.begin(), foundScripts.end(), [&](const std::string & aStr, const std::string & bStr) {
		std::filesystem::path aPath(aStr);
		std::filesystem::path bPath(bStr);

		int aNamePriority = getNamePriority(aPath);
		int bNamePriority = getNamePriority(bPath);

		if(aNamePriority != bNamePriority)
			return aNamePriority < bNamePriority;

		if(aNamePriority <= 1) {
			int aExtPriority = getExtPriority(aPath.extension().string());
			int bExtPriority = getExtPriority(bPath.extension().string());

			if(aExtPriority != bExtPriority)
				return aExtPriority < bExtPriority;
		}

		return aPath.filename().string() < bPath.filename().string();
	});

	return foundScripts;
}

json HandyAPIClient::upload(const std::string &scriptPath, const std::string &targetUrl) {
	CURL *curl = curl_easy_init();
	if(!curl)
		return json::object();

	std::string responseString;
	curl_mime *mime = curl_mime_init(curl);
	curl_mimepart *part = curl_mime_addpart(mime);

	curl_mime_name(part, "file");
	curl_mime_filedata(part, scriptPath.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, targetUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

	CURLcode res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
		std::cerr << "Upload failed: " << curl_easy_strerror(res) << std::endl;
		return json::object();
	}

	long httpCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	if(httpCode != 200) {
		std::cerr << "Upload failed with HTTP status code: " << httpCode << std::endl;
		return json::object();
	}

	curl_mime_free(mime);
	curl_easy_cleanup(curl);

	try {
		return json::parse(responseString);
	} catch(const json::parse_error&) {
		std::cerr << "Malformed JSON response payload received from endpoint.\n";
		return json::object();
	}
}

std::string HandyAPIClient::makeHTTPRequest(const std::string &method, const std::string &url, const std::string &bodyData) {
	CURL *curl = curl_easy_init();
	std::string responseString;

	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

		struct curl_slist *chunk = nullptr;
		for(const auto &[key, value] : headers) {
			std::string headerStr = key + ": " + value;
			chunk = curl_slist_append(chunk, headerStr.c_str());
		}

		if(!bodyData.empty()) {
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyData.c_str());
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

		CURLcode res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			std::cerr << "CURL Request Failed: " << curl_easy_strerror(res) << std::endl;
		}

		long httpCode = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

		if(httpCode < 200 || httpCode >= 300) {
			std::cerr << "HTTP Request Failed: "
					  << httpCode
					  << std::endl;
		}

		curl_slist_free_all(chunk);
		curl_easy_cleanup(curl);
	}
	return responseString;
}

HandyAPIClient::HandyAPIClient(const std::string &connKey, HandyAPIClient::FirmwareVersion APIFirmwareVersion, const std::string &connAuth) : apiConnKey(connKey), apiAuthKey(connAuth), firmwareVersion(APIFirmwareVersion) {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	headers["X-Connection-Key"] = connKey;

	switch(APIFirmwareVersion) {
		case FW3:
			apiEndpoint = apiEndpointFW3;
			break;

		case FW4:
			apiEndpoint = apiEndpointFW4;
			headers["X-Api-Key"] = connAuth;
			break;

		default:
			apiEndpoint = apiEndpointFW3;
			break;
	}
}

json getResponse(const std::string &responseString, HandyAPIClient::FirmwareVersion firmwareVersion) {
	switch(firmwareVersion) {
		case HandyAPIClient::FirmwareVersion::FW3:
			return json::parse(responseString);
		case HandyAPIClient::FirmwareVersion::FW4: {
			json data = json::parse(responseString);

			if(data.contains("result") && !data["result"].is_null())
				return data["result"];
			else
				return data;
		};
	}

	return json::object();
}

HandyAPIClient::~HandyAPIClient() {
	curl_global_cleanup();
}

bool HandyAPIClient::checkStatus() {
	std::string response = makeHTTPRequest("GET", apiEndpoint + "mode");
	try {
		json data = getResponse(response, firmwareVersion);
		if(data.contains("mode") && !data["mode"].is_null())
			return true;
	} catch(...) {
	}
	return false;
}

void HandyAPIClient::setMode(int mode) {
	json body;
	body["mode"] = mode;
	makeHTTPRequest("PUT", apiEndpoint + (firmwareVersion == HandyAPIClient::FirmwareVersion::FW3 ? "mode" : "mode2"), body.dump());
}

long long HandyAPIClient::getServerTime() {
	return getCurrentTimeMillis() + averageTimeOffset;
}

void HandyAPIClient::synchronizeTime() {
	std::vector<TimeSample> samples;
	const int requiredSamples = 25;
	const int discardCount = 10;
	const int totalNeeded = requiredSamples + discardCount;

	while(samples.size() < totalNeeded) {
		long long requestStart = getCurrentSteadyMillis();
		std::string response = makeHTTPRequest("GET", apiEndpoint + "servertime");
		long long requestEnd = getCurrentSteadyMillis();
		long long requestEndReal = getCurrentTimeMillis();

		try {
			json data = json::parse(response);
			std::string timeKey = firmwareVersion == HandyAPIClient::FirmwareVersion::FW3 ? "serverTime" : "server_time";
			if(data.contains(timeKey)) {
				long long serverTime = data[timeKey].get<long long>();
				long long roundTrip = requestEnd - requestStart;
				long long offset = (serverTime + (roundTrip / 2)) - requestEndReal;

				samples.push_back({offset, roundTrip});
			}
		} catch(...) {
		}
	}

	std::sort(samples.begin(), samples.end(), [](const TimeSample & a, const TimeSample & b) {
		return a.roundTrip < b.roundTrip;
	});

	long long totalOffset = 0;
	for(int i = 0; i < requiredSamples; i++) {
		totalOffset += samples[i].offset;
	}

	averageTimeOffset = totalOffset / requiredSamples;
	timeSyncCount = requiredSamples;
}

void HandyAPIClient::setupScript(const std::string &scriptURL) {
	json body;
	body["url"] = scriptURL;
	makeHTTPRequest("PUT", apiEndpoint + "hssp/setup", body.dump());
}

void HandyAPIClient::playScript(long long startTime) {
	json body;
	std::string serverTimeKey = firmwareVersion == HandyAPIClient::FirmwareVersion::FW3 ? "estimatedServerTime" : "server_time";
	std::string startTimeKey = firmwareVersion == HandyAPIClient::FirmwareVersion::FW3 ? "startTime" : "start_time";
	body[serverTimeKey] = getServerTime();
	body[startTimeKey] = startTime;

	if(firmwareVersion == HandyAPIClient::FirmwareVersion::FW4)
		body["playback_rate"] = currentSpeed;

	makeHTTPRequest("PUT", apiEndpoint + "hssp/play", body.dump());
}

void HandyAPIClient::stopScript() {
	makeHTTPRequest("PUT", apiEndpoint + "hssp/stop");
}
