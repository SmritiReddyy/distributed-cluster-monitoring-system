#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <mutex>
#include <string>

class Logger {
    std::mutex logMutex;
    std::ofstream file;
public:
    Logger(const std::string &filename);
    ~Logger();
    void info(const std::string &msg);
    void warn(const std::string &msg);
};

#endif
