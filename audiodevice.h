#pragma once

#include <alsa/asoundlib.h>

#include "result.h"
#include "mediaformat.h"

namespace audiodevice
{
    struct Device
    {
        snd_pcm_t* playbackHandle;
    };

    Result Create(Device*& device, uint32_t channels, uint32_t sampleRate, SampleFormat sampleFormat);
    Result WriteInterleaved(Device* device, void* buf, uint32_t frames);
}


