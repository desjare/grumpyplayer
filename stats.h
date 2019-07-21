#pragma once

namespace stats
{
    
    struct Profiler
    {
         uint64_t avgTime;
         uint32_t count;
         uint64_t minTime
         uint64_t maxTime
    };

    void StartProfiler(int profiler);
    void StopProfiler(int profiler);


};
