
#include "stats.h"
#include "chrono.h"

#include <iostream>
#include <sstream>
#include <string.h>

namespace {

    stats::Profiler profilers[stats::PROFILER_NB];
    bool enable = false;
}

namespace stats
{
    void Init()
    {
        memset(&profilers, 0, sizeof(Profiler) * PROFILER_NB);
    }

    void Enable(bool e)
    {
        enable = e;
    }

    void GetPointName(Point point, std::string& name)
    {
        switch(point)
        {
            case PROFILER_VIDEO_DRAW:
                name = "vdraw";
                break;
            case PROFILER_AUDIO_WRITE:
                name = "awrite";
                break;
            case PROFILER_DECODE_VIDEO_FRAME:
                name = "vdec";
                break;
            case PROFILER_DECODE_AUDIO_FRAME:
                name = "adec";
                break;
            case PROFILER_PROCESS_VIDEO_FRAME:
                name = "vproc";
                break;
            case PROFILER_PROCESS_AUDIO_FRAME:
                name = "aproc";
                break;
            case PROFILER_NB:
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
        p.endTime = chrono::Now();
        p.currentTime = p.endTime - p.startTime;

        if( p.avgTime == 0 )
        {
            p.avgTime = p.currentTime;
        }
        else
        {
            double sums = p.avgTime * (p.count-1) + p.currentTime;
            p.avgTime =  static_cast<uint64_t>(sums / static_cast<double>(p.count));
        }

        if( p.minTime == 0 )
        {
            p.minTime = p.currentTime;
            
        }
        else
        {
            p.minTime = std::min(p.minTime,p.currentTime);
        }

        p.maxTime = std::max(p.maxTime,p.currentTime);
    }

    void PrintStats()
    {
        if( !enable )
        {
            return;
        }

        std::ostringstream out;

        out << "(curr(ms), avg(ms)) ";

        for(uint32_t i = 0; i < PROFILER_NB; i++)
        {
            Profiler& p = profilers[i];

            std::string name;
            GetPointName(static_cast<Point>(i), name);

            out << name << " (" << chrono::Milliseconds(p.currentTime) << "," << chrono::Milliseconds(p.avgTime) << ") ";
        }
        out << std::endl;

        std::cerr << out.str();
    }
}
