#pragma once

#include <httplib.h>
#include <mutex>
#include <string>

class HandyServer {
  public:
	explicit HandyServer(int port = 8008 /*Boob*/);

	~HandyServer();

	void start();

	std::string hostScript(const std::string &scriptPath);

  private:
	std::thread serverThread;
	std::string loadScript(const std::string &path);

	std::string generateToken();

	std::string getLocalIP();

	httplib::Server server;

	int port;

	std::string currentCSV;
	std::string currentToken;

	std::mutex mutex;

	bool running = false;
};
