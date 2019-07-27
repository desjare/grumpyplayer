#pragma once

#include <chrono>
#include <thread>
#include <stdio.h>

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
    inline double Seconds(uint64_t timeUs)
    {
        double t = static_cast<double>(timeUs);
        return t / 1000000.0;
    }

    // convert microseconds to milliseconds 
    inline double Milliseconds(uint64_t timeUs)
    {
        double t = static_cast<double>(timeUs);
        return t / 1000.0;
    }

    // convert seconds to microseconds
    inline uint64_t Microseconds(double seconds)
    {
        return static_cast<uint64_t>(seconds * 1000000.0);
    }
}
