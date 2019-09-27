
#pragma once

#include "videodevice.h"
#include "audiodevice.h"
#include "mediadecoder.h"

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 26495) // uninitialized variable
#endif
#include <boost/function.hpp>
#ifdef WIN32
#pragma warning( pop ) 
#endif

#include <thread>

namespace player
{
    typedef boost::function<void ()> SwapBufferCallback;

    struct Player
    {
        std::string path;

        mediadecoder::Decoder* decoder = nullptr;
        
        mediadecoder::Producer* producer = nullptr;
        mediadecoder::VideoFrame* videoFrame = nullptr;
        mediadecoder::Subtitle* subtitle = nullptr;
        mediadecoder::Subtitle* nextSubtitle = nullptr;

        audiodevice::Device* audioDevice = nullptr;
        videodevice::Device* videoDevice = nullptr;

        uint64_t playbackStartTimeUs = 0;
        uint64_t currentTimeUs = 0;

        std::atomic<bool> playing = false;
        std::atomic<bool> pause = false;
        std::atomic<bool> buffering = false;

        std::atomic<bool> queueAudio = false;;
        std::thread audioThread;
    };

    Result   Init(SwapBufferCallback);
    Result   Create(Player*& player);
    Result   Open(Player*, const std::string& filename);
    void     SetWindowSize(Player*,uint32_t, uint32_t);
    void     ToggleSubtitleTrack(Player*);

    void     Play(Player*);
    void     Seek(Player*, uint64_t timeUs);
    void     Pause(Player*);

    bool     IsPlaying(Player*);

    uint64_t GetDuration(Player*);
    uint64_t GetCurrentTime(Player*);

    const std::string& GetPath(Player*);

    
    void     Present(Player*);
    
    void     Close(Player*);
    void     Destroy(Player*&);


};
