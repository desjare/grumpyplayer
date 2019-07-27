#pragma once

#include <string>

namespace logger
{
    enum Level
    {
        DEBUG = 0,
        INFO,
        WARNING,
        ERROR,
        INVALID
    };

    void  SetLevel(int level);
    Level GetLevelFromString(const std::string&);

    void Debug(const char* fmt, ...);
    void Info(const char* fmt, ...);
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);
}
