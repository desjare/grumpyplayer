
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>

namespace logger
{
    Level logLevel = INFO;

    void SetLevel(int level)
    {
        logLevel = static_cast<Level>(level);
    }

    Level GetLevelFromString(const std::string& str)
    {
        Level level = INVALID;

        if( str == "debug" )
        {
            level = DEBUG;
        }
        else if( str == "info" )
        {
            level = INFO;
        }
        else if( str == "warning" )
        {
            level = WARNING;
        }
        else if( str == "error" )
        {
            level = ERROR;
        }

        return level;
    }

    void Debug(const char* fmt, ...)
    {
      if(logLevel != DEBUG )
      {
          return;
      }
      va_list args;
      char buffer[BUFSIZ];

      va_start(args, fmt);
      vsnprintf(buffer, sizeof buffer, fmt, args);
      va_end(args);

      printf("Debug: %s\n", buffer);
    }

    void Info(const char* fmt, ...)
    {
      if(logLevel > INFO )
      {
          return;
      }

      va_list args;
      char buffer[BUFSIZ];

      va_start(args, fmt);
      vsnprintf(buffer, sizeof buffer, fmt, args);
      va_end(args);

      printf("Info: %s\n", buffer);
    }

    void Warn(const char* fmt, ...)
    {
      if(logLevel > WARNING )
      {
          return;
      }
      va_list args;
      char buffer[BUFSIZ];

      va_start(args, fmt);
      vsnprintf(buffer, sizeof buffer, fmt, args);
      va_end(args);

      fprintf(stderr, "Warn: %s\n", buffer);
    }

    void Error(const char* fmt, ...)
    {
      if(logLevel > ERROR )
      {
          return;
      }

      va_list args;
      char buffer[BUFSIZ];

      va_start(args, fmt);
      vsnprintf(buffer, sizeof buffer, fmt, args);
      va_end(args);

      fprintf(stderr, "Error: %s\n", buffer);
    }


}
