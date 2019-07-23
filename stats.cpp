
#include "stats.h"
#include "chrono.h"

#include <iostream>
#include <sstream>
#include <string.h>

namespace {

    stats::Profiler profilers[stats::PROFILER_NB];
}

namespace stats
{
    void Init()
    {
        memset(&profilers, 0, sizeof(Profiler) * PROFILER_NB);
    }

    void GetPointName(Point point, std::string& name)
    {
        switch(point)
        {
            case PROFILER_VIDEO_DRAW:
                name = "Video Draw";
                break;
            case PROFILER_AUDIO_WRITE:
                name = "Audio Write";
                break;
        }
    }

    void StartProfiler(Point profiler)
    {
        uint64_t currentTime = chrono::Now();
        Profiler& p = profilers[profiler];
        p.startTime = currentTime;
        p.count++;
    }

    void StopProfiler(Point profiler)
    {
        Profiler& p = profilers[profiler];
        uint64_t currentTime = chrono::Now();
        uint64_t elapsedTime = currentTime - p.startTime;

        p.startTime = currentTime;
        
        if( p.avgTime == 0 )
        {
            p.avgTime = elapsedTime;
        }
        else
        {
            p.avgTime = ((p.avgTime * (p.count-1)) + elapsedTime) / p.count;
        }

        if( p.minTime == 0 )
        {
            p.minTime = elapsedTime;
            
        }
        else
        {
            p.minTime = std::min(p.minTime,elapsedTime);
        }

        p.maxTime = std::min(p.maxTime,elapsedTime);
    }

    void PrintPoints()
    {
        std::ostringstream out;

        for(uint32_t i = 0; i < PROFILER_NB; i++)
        {
            Profiler& p = profilers[i];

            std::string name;
            GetPointName(static_cast<Point>(i), name);

            out << name << " " << chrono::Seconds(p.avgTime) << " avg(s) ";
        }
        out << std::endl;

        std::cerr << out.str();
    }
}
