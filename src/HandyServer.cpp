#include "HandyServer.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

HandyServer::HandyServer(int port) : port(port) {

	server.Get(R"(/scripts/([A-Za-z0-9]+))",
	[this](const httplib::Request & req, httplib::Response & res) {
		std::lock_guard<std::mutex> lock(mutex);

		if(req.matches.size() < 2 || req.matches[1] != currentToken) {
			res.status = 404;
			return;
		}

		res.set_header("Cache-Control", "no-cache");

		res.set_content(currentCSV, "text/csv");
	});
}

HandyServer::~HandyServer() {
	if(running) {
		server.stop();
		if(serverThread.joinable()) {
			serverThread.join();
		}
	}
}

void HandyServer::start() {
	if(running)
		return;

	running = true;

	serverThread = std::thread([this]() {
		server.listen("0.0.0.0", port);
	});

	server.wait_until_ready();

	if(!server.is_running()) {
		running = false;
		std:: cerr << "Failed to start server on port " << port << std::endl;
	}
}

std::string HandyServer::generateToken() {
	static const char chars[] = "abcdefghijklmnopqrstuvwxyz"
								"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
								"0123456789";

	std::random_device rd;
	std::mt19937 gen(rd());

	std::string token;

	for(int i = 0; i < 32; i++) {
		token += chars[gen() % (sizeof(chars) - 1)];
	}

	return token;
}

std::string HandyServer::loadScript(const std::string &path) {
	std::filesystem::path filePath(path);

	std::ifstream file(path);

	if(!file) {
		std::cerr << "Cannot open script: " << path << std::endl;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string rawContent = buffer.str();

	if(filePath.extension() == ".csv") {
		return rawContent;
	}

	auto data = json::parse(rawContent, nullptr, false);

	if(data.is_discarded()) {
		return rawContent;
	}

	if(!data.contains("actions") || !data["actions"].is_array()) {
		std::cerr << "Invalid script format: Missing actions array" << std::endl;
	}

	std::ostringstream csv;

	auto formatNumber = [](double v) -> std::string {
		long long rounded = static_cast<long long>(std::floor(v + 0.5));
		return std::to_string(rounded);
	};

	for(const auto &action : data["actions"]) {
		if(!action.is_object() ||
				!action.contains("at") || action["at"].is_null() ||
				!action.contains("pos") || action["pos"].is_null()) {
			std::cerr << "Invalid action item inside payload" << std::endl;
		}

		double at = action["at"].get<double>();
		double pos = action["pos"].get<double>();

		csv << formatNumber(at) << "," << formatNumber(pos) << "\n";
	}

	return csv.str();
}

std::string HandyServer::getLocalIP() {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	if(sock < 0)
		return "127.0.0.1";

	sockaddr_in remote{};

	remote.sin_family = AF_INET;
	remote.sin_port = htons(53);

	inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

	connect(sock, (sockaddr*)&remote, sizeof(remote));

	sockaddr_in local{};

	socklen_t len = sizeof(local);

	getsockname(sock, (sockaddr*)&local, &len);

	close(sock);

	char buffer[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &local.sin_addr, buffer, sizeof(buffer));

	return std::string(buffer);
}

std::string HandyServer::hostScript(const std::string &scriptPath) {
	std::string newCSV = loadScript(scriptPath);

	std::lock_guard<std::mutex> lock(mutex);

	currentCSV = std::move(newCSV);

	currentToken = generateToken();

	return "http://" + getLocalIP() + ":" + std::to_string(port) + "/scripts/" +
		   currentToken;
}
