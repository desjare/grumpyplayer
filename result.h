
#pragma once

#include <string>
#include <cstdio>
#include <stdarg.h>

class Result
{
public:
    Result()
    : status(true)
    {
    }

    Result(bool status) 
    : status(status)
    {
    }

    Result(bool status, const std::string& error)
    : status(status),
      error(error)
    {
    }

    Result(bool status, const char* fmt, ...)
    : status(status)
    {
        va_list args;
        char buffer[BUFSIZ];

        va_start(args,fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        error = buffer;
    }

    Result(const Result& rhs)
    : status(rhs.status),
      error(rhs.error)
    {
    }

    Result& operator=(const Result& rhs)
    {
        status = rhs.status;
        error = rhs.error;
        return *this;
    }

    operator bool() const
    {
        return status;
    }

    bool getStatus() const
    {
        return status;
    }

    const std::string& getError() const 
    { 
        return error; 
    }

protected:
    bool status;
    std::string error;
};
