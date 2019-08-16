#pragma once

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#ifdef WIN32
#include <xaudio2.h>
#endif

#include "result.h"
#include "mediaformat.h"

#include <atomic>


namespace audiodevice
{
#ifdef WIN32
    class Audio2VoiceCallback;
#endif

    static const int64_t ENQUEUE_SAMPLES_US = 10000000;

    struct Device
    {
#ifdef HAVE_ALSA
        snd_pcm_t* playbackHandle;
#endif

#ifdef WIN32
        IXAudio2* xaudioHandle;
        IXAudio2MasteringVoice* masterVoice;
        IXAudio2SourceVoice* sourceVoice;
        WAVEFORMATEX wfx;
        Audio2VoiceCallback* voiceCallbacks;
#endif
    };

    Result Create(Device*& device);
    void   Destroy(Device*& device);

    Result SetInputFormat(Device* device, uint32_t channels, uint32_t sampleRate, SampleFormat sampleFormat);

    Result WriteInterleaved(Device* device, void* buf, uint32_t frames, std::atomic<bool>& bufferInUse);
    
    Result StartWhenReady(Device* device);

    Result Pause(Device*);
    Result Resume(Device*);
    Result Flush(Device*);

}


