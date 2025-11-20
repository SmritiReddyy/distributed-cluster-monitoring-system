#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <string>
#include <ctime>

inline std::string timestamp() {
    time_t now = time(0);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&now));
    return std::string(buf);
}

#endif