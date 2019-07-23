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

    // convert seconds to microseconds
    inline uint64_t Microseconds(double seconds)
    {
        return static_cast<uint64_t>(seconds * 1000000.0);
    }

    inline bool WaitForPlayback(const char* name, uint64_t startTimeUs, uint64_t timeUs, uint64_t waitThresholdUs)
    {
        const int64_t sleepThresholdUs = 10000;
        const int64_t millisecondUs = 1000;
        const int64_t logDeltaThresholdUs = 100;

        int64_t waitTime = Wait(startTimeUs, timeUs);

        if( waitTime <= 0 )
        {
            fprintf(stderr, "WaitForPlayback. %s Late Playback. %f\n", name, Seconds(waitTime));
            return true;
        }
        else
        {
            if( waitTime >= sleepThresholdUs )
            {
                std::this_thread::sleep_for(std::chrono::microseconds(waitTime-millisecondUs));
                waitTime = Wait(startTimeUs, timeUs);

                if( waitTime <= 0 )
                {
                    return true;
                }
            }

            if( waitTime > 0 )
            {
                do
                {
                    std::this_thread::yield();
                    waitTime = Wait(startTimeUs, timeUs);

                } while( waitTime >= waitThresholdUs );
            } 

            const uint64_t currentTimeUs = Current(startTimeUs);
            const int64_t deltaUs = std::abs( static_cast<int64_t>(currentTimeUs-timeUs) );

            if( deltaUs > logDeltaThresholdUs )
            {
                fprintf(stderr, "WaitForPlayback %s play time %ld us decode %ld us diff %ld\n", name, currentTimeUs, timeUs, deltaUs );
            }
        }
        return true;
    }

}
