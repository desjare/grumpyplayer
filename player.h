
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
        
        mediadecoder::producer::Producer* producer;
        mediadecoder::producer::VideoFrame* videoFrame;

        audiodevice::Device* audioDevice;
        videodevice::Device* videoDevice;
        SwapBufferCallback swapBufferCallback;

        bool play;
        uint64_t playbackStartTimeUs;

        std::thread audioThread;
    };

    Result Init(SwapBufferCallback);

    Result Create(Player*& player, mediadecoder::Decoder*, audiodevice::Device* audioDevice, videodevice::Device* videoDevice);
    
    void   Play(Player*);
    void   Present(Player*);


};
