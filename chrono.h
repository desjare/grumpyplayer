#pragma once

#include <chrono>
#include <thread>
#include <stdio.h>
#include <inttypes.h>

namespace chrono
{
    // epoch time in us
    inline uint64_t Now()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    // elapsed time since start
    inline uint64_t Current(uint64_t startTimeUs)
    {
        return Now() - startTimeUs;
    }

      // time to wait for presentation time
    inline int64_t Wait(uint64_t startTimeUs, uint64_t presentationTimeUs)
    {
        return presentationTimeUs - Current(startTimeUs);
    }

    // convert microseconds to seconds
    template <typename T>
    inline double Seconds(T timeUs)
    {
        double t = static_cast<double>(timeUs);
        return t / 1000000.0;
    }

    // convert microseconds to milliseconds 
    template <typename T>
    inline double Milliseconds(T timeUs)
    {
        double t = static_cast<double>(timeUs);
        return t / 1000.0;
    }

    // convert seconds to microseconds
    inline uint64_t Microseconds(double seconds)
    {
        return static_cast<uint64_t>(seconds * 1000000.0);
    }

    inline void HoursMinutesSeconds(uint64_t timeUs, int64_t& hour, int64_t& min, int64_t& sec)
    {
        const int64_t secondsInHour = 3600;
        const int64_t secondsInMinute = 60;

        int64_t time = static_cast<int64_t>(Seconds(timeUs));

        hour = time / secondsInHour;
        time = time % secondsInHour;

        min = time / secondsInMinute;
        time = time % secondsInMinute;
        sec = time;
    }

    inline std::string HoursMinutesSeconds(uint64_t timeUs)
    {
        int64_t hour = 0;
        int64_t min = 0;
        int64_t sec = 0;

        HoursMinutesSeconds(timeUs, hour, min, sec);

        char buf[BUFSIZ];
        if (hour != 0)
        {
            snprintf(buf, sizeof(buf), "%" PRId64 "h%" PRId64 "m%" PRId64 "s", hour, min, sec);
        }
        else if (min != 0)
        {
            snprintf(buf, sizeof(buf), "%" PRId64 "m%" PRId64 "s", min, sec);
        } 
        else
        {
            snprintf(buf, sizeof(buf), "%" PRId64 "s", sec);
        }
        return buf;
    }
}
