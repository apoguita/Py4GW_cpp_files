#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <unordered_map>

class Logger {
public:
    static Logger& Instance();

    // Logging methods
    bool LogInfo(const std::string& message);
	static bool LogStaticInfo(const std::string& message) {
		return Instance().LogInfo(message);
	}
    bool LogWarning(const std::string& message);
    bool LogError(const std::string& message);
    bool LogError(const std::string& message, const std::string& module_name);
    bool LogOk(const std::string& message);
    bool LogHook(const std::string& message);
    static bool AssertAddress(const std::string& name, uintptr_t address);
	static bool AssertAddress(const std::string& name, uintptr_t address, const std::string& module_name);
    static bool AssertHook(const std::string& name, int status);
    static bool AssertHook(const std::string& name, int status, const std::string& module_name);

    // Hook health: resolved scan addresses and hook statuses
    static const std::unordered_map<std::string, uintptr_t>& GetScanResults();
    static const std::unordered_map<std::string, int>& GetHookResults();

    // Initialize and terminate logging
    void SetLogFile(const std::string& file_path);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex log_mutex_;  // Mutex for thread-safe logging
    std::string log_file_path_;

    // Utility for formatted log entry
    std::string GetTimestamp() const;
    bool WriteLog(const std::string& level, const std::string& message);
};
