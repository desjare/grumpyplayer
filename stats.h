#pragma once

#include <string>
#include <stdint.h>

namespace stats
{
    enum Point
    {
        PROFILER_VIDEO_DRAW = 0,
        PROFILER_AUDIO_WRITE,
        PROFILER_DECODE_VIDEO_FRAME,
        PROFILER_DECODE_AUDIO_FRAME,
        PROFILER_PROCESS_VIDEO_FRAME,
        PROFILER_PROCESS_AUDIO_FRAME,
        PROFILER_NB
    };
    
    struct Profiler
    {
         uint64_t startTime;
         uint64_t endTime;
         uint64_t currentTime;
         uint64_t avgTime;
         uint64_t count;
         uint64_t minTime;
         uint64_t maxTime;
    };

    void Init();
    void GetPointName(Point, std::string&);
    void StartProfiler(Point profiler);
    void StopProfiler(Point profiler);
    void PrintPoints();

    class ScopeProfiler
    {
    public:
        ScopeProfiler(Point profiler)
        : type(profiler)
        {
            StartProfiler(type);
        }

        ~ScopeProfiler()
        {
            StopProfiler(type);
        }
    private:
        Point type;
    };


};
