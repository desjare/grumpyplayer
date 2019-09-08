#pragma once

#include <string>

namespace logger
{
    enum Level
    {  
        TRACE = 0,
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        INVALID
    };

    void  SetLevel(int level);
    Level GetLevelFromString(const std::string&);

    void Trace(const char* fmt, ...);
    void Debug(const char* fmt, ...);
    void Info(const char* fmt, ...);
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);
}
