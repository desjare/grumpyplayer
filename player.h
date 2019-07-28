
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
        SwapBufferCallback swapBufferCallback;

        uint64_t playbackStartTimeUs;

        std::atomic<bool> play;
        std::thread audioThread;
    };

    Result Init(SwapBufferCallback);

    Result Create(Player*& player, mediadecoder::Decoder*, audiodevice::Device* audioDevice, videodevice::Device* videoDevice);
    
    void   Play(Player*);
    Result Open(Player*, const std::string& filename);
    void   Present(Player*);
    void   Close(Player*);
    void   Destroy(Player*);


};
