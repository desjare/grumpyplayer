
#pragma once

#include "videodevice.h"
#include "audiodevice.h"
#include "mediadecoder.h"

#include <boost/function.hpp>
#include <thread>

namespace player
{
    typedef boost::function<void ()> SwapBufferCallback;

    struct Player
    {
        mediadecoder::Decoder* decoder;
        
        mediadecoder::Producer* producer;
        mediadecoder::VideoFrame* videoFrame;

        audiodevice::Device* audioDevice;
        videodevice::Device* videoDevice;

        uint64_t playbackStartTimeUs;
        uint64_t currentTimeUs;
        std::atomic<bool> playing;

        std::atomic<bool> queueAudio;
        std::thread audioThread;
    };

    Result   Init(SwapBufferCallback);
    Result   Create(Player*& player);
    Result   Open(Player*, const std::string& filename);

    void     Play(Player*);
    void     Seek(Player*, uint64_t timeUs);
    void     Pause(Player*);

    bool     IsPlaying(Player*);
    uint64_t GetDuration(Player*);
    uint64_t GetCurrentTime(Player*);
    
    void     Present(Player*);
    
    void     Close(Player*);
    void     Destroy(Player*&);


};
