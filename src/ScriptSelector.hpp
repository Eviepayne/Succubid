#ifndef SCRIPT_SELECTOR_HPP
#define SCRIPT_SELECTOR_HPP

#include "MpvIPC.hpp"
#include "succubid_selector.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

class ScriptSelector {
  public:
	explicit ScriptSelector(MpvIPC& ipc)
		: m_ipc(ipc) {
		m_ipc.onCustomMessage(
		[this](const std::string & target, const std::vector<std::string>& args) {
			if(target == "succubid_pong") {
				m_pongReceived.store(true);
			} else if(target == "selection_response" && args.size() >= 2) {
				try {
					m_selectedIndex.store(std::stoi(args[1]));
				} catch(...) {
					m_selectedIndex.store(-1);
				}
			}
		});
	}

	bool ensure_helper(bool forceReload = false) {
		if(!m_ipc.isConnected()) {
			std::cerr << "[ScriptSelector] Cannot inject script: MPV is not connected.\n";
			return false;
		}

		if(!forceReload && is_helper_alive()) {
			return true;
		}

		try {
			fs::path tempDir = fs::temp_directory_path();
			fs::path tempScriptPath = tempDir / "succubid_selector.lua";

			std::ofstream outFile(tempScriptPath);
			if(!outFile.is_open()) {
				std::cerr << "[ScriptSelector] Failed to write temp file: "
						  << tempScriptPath << "\n";
				return false;
			}

			outFile.write(
				reinterpret_cast<const char*>(succubid_selector_lua),
				succubid_selector_lua_len);
			outFile.close();

			m_ipc.sendCommand(
			{{"command", {"load-script", tempScriptPath.string()}}});

			if(!wait_for_helper(std::chrono::milliseconds(500))) {
				std::cerr << "[ScriptSelector] Helper failed to start.\n";

				std::error_code ec;
				fs::remove(tempScriptPath, ec);
				return false;
			}

			std::error_code ec;
			fs::remove(tempScriptPath, ec);

			return true;
		} catch(const std::exception& e) {
			std::cerr
					<< "[ScriptSelector] Exception during dynamic script injection: "
					<< e.what() << "\n";
			return false;
		}
	}

	std::optional<size_t> select(
		const std::vector<std::string>& options,
		std::chrono::milliseconds timeoutDuration =
			std::chrono::seconds(30)) {
		if(options.empty())
			return std::nullopt;
		if(options.size() == 1)
			return 0;
		if(!m_ipc.isConnected())
			return std::nullopt;

		if(!ensure_helper())
			return std::nullopt;

		m_selectedIndex.store(-2);

		std::vector<std::string> payload = {"show_selection_menu"};
		payload.insert(payload.end(), options.begin(), options.end());
		m_ipc.sendCustomMessage(payload);

		auto startTime = std::chrono::steady_clock::now();

		while(m_selectedIndex.load() == -2 && m_ipc.isConnected()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));

			if(timeoutDuration.count() > 0) {
				auto elapsed =
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - startTime);

				if(elapsed >= timeoutDuration) {
					m_ipc.sendCustomMessage({"succubid_cancel"});
					return std::nullopt;
				}
			}
		}

		int result = m_selectedIndex.load();

		if(result < 0 || result >= static_cast<int>(options.size()))
			return std::nullopt;

		return static_cast<size_t>(result);
	}

  private:
	bool wait_for_helper(std::chrono::milliseconds timeout) {
		auto start = std::chrono::steady_clock::now();

		while(m_ipc.isConnected()) {
			m_pongReceived.store(false);

			m_ipc.sendCustomMessage({"succubid_ping"});

			auto pingStart = std::chrono::steady_clock::now();

			while(!m_pongReceived.load() && m_ipc.isConnected()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));

				if(std::chrono::steady_clock::now() - pingStart >=
						std::chrono::milliseconds(20))
					break;
			}

			if(m_pongReceived.load())
				return true;

			if(std::chrono::steady_clock::now() - start >= timeout)
				break;
		}

		return false;
	}

	bool is_helper_alive() {
		return wait_for_helper(std::chrono::milliseconds(50));
	}

	MpvIPC& m_ipc;

	std::atomic<bool> m_pongReceived{false};
	std::atomic<int> m_selectedIndex{-2};
};

#endif // SCRIPT_SELECTOR_HPP
