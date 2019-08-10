#pragma once

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "result.h"
#include "mediaformat.h"

namespace audiodevice
{
    static const int64_t ENQUEUE_SAMPLES_US = 10000000;

    struct Device
    {
#ifdef HAVE_ALSA
        snd_pcm_t* playbackHandle;
#endif
    };

    Result Create(Device*& device);
    void   Destroy(Device*& device);

    Result SetInputFormat(Device* device, uint32_t channels, uint32_t sampleRate, SampleFormat sampleFormat);
    
    Result WriteInterleaved(Device* device, void* buf, uint32_t frames);
    
    Result Drop(Device*);
    Result Pause(Device*);
    Result Resume(Device*);

}


