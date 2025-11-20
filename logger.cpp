// logger.cpp
#include "logger.hpp"
#include "utils.hpp"

Logger::Logger(const std::string &filename) {
    file.open(filename, std::ios::app);
}
Logger::~Logger() {
    if (file.is_open()) file.close();
}

void Logger::info(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    file << "[" << timestamp() << "] [INFO] " << msg << std::endl;
    std::cout << "[INFO] " << msg << std::endl;
}

void Logger::warn(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    file << "[" << timestamp() << "] [WARN] " << msg << std::endl;
    std::cout << "[WARN] " << msg << std::endl;
}