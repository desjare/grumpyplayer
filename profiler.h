#pragma once

#include <string>
#include <stdint.h>

namespace profiler
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
    
    // simple flat profiler that keeps track
    // of average time for a given profile point
    struct Profiler
    {
         uint64_t startTime = 0;
         uint64_t endTime = 0;
         uint64_t currentTime = 0;
         uint64_t totalTime = 0;
         uint64_t count = 0;
         uint64_t minTime = 0;
         uint64_t maxTime = 0;
    };

    void Init();
    void Enable(bool);

    void GetPointName(Point, std::string&);

    // Start a profiler block
    void StartBlock(Point profiler);
    void StopBlock(Point profiler);

    // Print profiler point stats
    void Print();

    class ScopeProfiler
    {
    public:
        ScopeProfiler(Point profiler)
        : type(profiler)
        {
            StartBlock(type);
        }

        ~ScopeProfiler()
        {
            StopBlock(type);
        }
    private:
        Point type;
    };


};
