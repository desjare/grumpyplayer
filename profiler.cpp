#include "precomp.h"
#include "profiler.h"
#include "chrono.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <string.h>


namespace {

    profiler::Profiler profilers[profiler::PROFILER_NB];
    bool enable = false;

    double Average(uint64_t totalTime, uint64_t count)
    {
        return static_cast<double>(totalTime) / static_cast<double>(count);
    }
}

namespace profiler
{
    void Init()
    {
        memset(&profilers, 0, sizeof(Profiler) * PROFILER_NB);
        for(uint32_t i = 0; i < PROFILER_NB; i++)
        {
            profilers[i].minTime = std::numeric_limits<uint64_t>::max();
        }
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

    void StartBlock(Point profiler)
    {
        if( !enable )
        {
            return;
        }

        uint64_t currentTime = chrono::Now();
        Profiler& p = profilers[profiler];
        p.startTime = currentTime;
        p.count++;
    }

    void StopBlock(Point profiler)
    {
        if( !enable )
        {
            return;
        }

        Profiler& p = profilers[profiler];
        p.endTime = chrono::Now();
        p.currentTime = p.endTime - p.startTime;
        p.totalTime += p.currentTime;
        p.minTime = std::min(p.minTime,p.currentTime);
        p.maxTime = std::max(p.maxTime,p.currentTime);
    }

    void Print()
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

            double currentTime = chrono::Milliseconds(p.currentTime);
			double averageTimeUs = Average(p.totalTime, p.count);
            double averageTime = chrono::Milliseconds(static_cast<uint64_t>(averageTimeUs));

            out << name << " (" << currentTime << "," << averageTime << ") ";
        }
        out << std::endl;

        std::cerr << out.str();
    }
}
