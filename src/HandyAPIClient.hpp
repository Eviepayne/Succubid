#ifndef HANDY_API_CLIENT_HPP
#define HANDY_API_CLIENT_HPP

#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class HandyAPIClient {
  public:
	enum FirmwareVersion {
		FW3,
		FW4
	};

  private:
	std::string apiConnKey;
	std::string apiAuthKey;
	std::string apiEndpoint;
	HandyAPIClient::FirmwareVersion firmwareVersion = FirmwareVersion::FW3;
#ifdef DEBUG_BUILD
	const std::string apiEndpointFW3 = "http://localhost:8080/fw3/";
	const std::string apiEndpointFW4 = "http://localhost:8080/fw4/";
#else
	const std::string apiEndpointFW3 = "https://www.handyfeeling.com/api/handy/v2/";
	const std::string apiEndpointFW4 = "https://www.handyfeeling.com/api/hand-rest/v3/";
#endif
	std::map<std::string, std::string> headers;

	long long averageTimeOffset = 0;
	long long aggregateTimeOffset = 0;
	int timeSyncCount = 0;
	double currentSpeed = 1.0;

	long long getCurrentTimeMillis();
	long long getCurrentSteadyMillis();
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
	std::string makeHTTPRequest(const std::string &method, const std::string &url, const std::string &bodyData = "");

	struct TimeSample {
		long long offset;
		long long roundTrip;
	};

  public:
	HandyAPIClient(const std::string &connKey, HandyAPIClient::FirmwareVersion APIFirmwareVersion = FW3, const std::string &connAuth = "");
	~HandyAPIClient();

	bool checkStatus();
	void setMode(int mode);
	long long getServerTime();
	void synchronizeTime();
	void setupScript(const std::string &scriptURL);
	void playScript(long long startTime);
	void stopScript();
	std::vector<std::string> findScripts(const std::string &videoPath);
	json upload(const std::string &scriptPath, const std::string &targetUrl);
	void setCurrentSpeed(double speed) {
		currentSpeed = speed;
	}
};

#endif // HANDY_API_CLIENT_HPP
