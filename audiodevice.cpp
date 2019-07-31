

#include "audiodevice.h"

namespace {
    snd_pcm_format_t SampleFormatToASound(SampleFormat sf)
    {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        const bool littleEndian = true;
    #elif  __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        const bool littleEndian = false;
    #elif
    #error "__BYTE_ORDER__ not defined"
    #endif

        snd_pcm_format_t sndFormat = SND_PCM_FORMAT_UNKNOWN;
        switch(sf)
        {
            case SF_FMT_U8:
                sndFormat = SND_PCM_FORMAT_U8;
                break;
            case SF_FMT_S16:
                sndFormat =  littleEndian ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_S16_BE;
                break;
            case SF_FMT_S32:
                sndFormat = littleEndian ? SND_PCM_FORMAT_S32_LE : SND_PCM_FORMAT_S32_BE;
                break;
            case SF_FMT_FLOAT:
                sndFormat = littleEndian ? SND_PCM_FORMAT_FLOAT_LE : SND_PCM_FORMAT_FLOAT_BE;
                break;
            case SF_FMT_DOUBLE:
                sndFormat = littleEndian ? SND_PCM_FORMAT_FLOAT64_LE : SND_PCM_FORMAT_FLOAT64_BE;
                break;
            default:
                break;
        }
        return sndFormat;
    }
}

namespace audiodevice
{
    Result Create(Device*& device, uint32_t channels, uint32_t sampleRate, SampleFormat sampleFormat)
    {
        Result result;
        snd_pcm_hw_params_t* hwParams = NULL;

        device = new Device();

        int err = snd_pcm_open(&device->playbackHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if( err < 0 )
        {
            result = Result(false, "Cannot open audio device %s", snd_strerror(err));
            return result;
        }

        err = snd_pcm_hw_params_malloc(&hwParams);
        if( err < 0 )
        {
            result = Result(false, "Cannot allocate hardware parameter structure %s", snd_strerror(err));
            return result;
        }
        
        err = snd_pcm_hw_params_any(device->playbackHandle, hwParams);
        if( err < 0 )
        {
            result = Result(false, "Cannot initialize parameter structure %s", snd_strerror(err));
            return result;
        }
         
        err = snd_pcm_hw_params_set_access(device->playbackHandle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
        if( err < 0 )
        {
            result = Result(false, "Cannot set access type %s", snd_strerror(err));
            return result;
        }

        err = snd_pcm_hw_params_set_format (device->playbackHandle, hwParams, SampleFormatToASound(sampleFormat));
        if( err < 0 )
        {
            result = Result(false, "Cannot set sample format %s", snd_strerror(err));
            return result;
        }

        err = snd_pcm_hw_params_set_rate_near(device->playbackHandle, hwParams, &sampleRate, 0);
        if( err < 0 )
        {
            result = Result(false, "Cannot set sample rate %s", snd_strerror(err));
            return result;
        }
        
        err = snd_pcm_hw_params_set_channels(device->playbackHandle, hwParams, channels);
        if( err < 0 )
        {
            result = Result(false, "Cannot set channel count %s", snd_strerror(err));
            return result;
        }

        err = snd_pcm_hw_params(device->playbackHandle, hwParams);
        if( err < 0 )
        {
            result = Result(false, "Cannot set parameters %s", snd_strerror(err));
            return result;
        }

        snd_pcm_hw_params_free(hwParams);
        
        err = snd_pcm_prepare(device->playbackHandle);
        if( err < 0 )
        {
            result = Result(false, "Cannot prepare audio interface for use %s", snd_strerror(err));
            return result;
        }

        return result;
    }

    Result WriteInterleaved(Device* device, void* buf, uint32_t frames)
    {
        Result result;
        snd_pcm_sframes_t written = snd_pcm_writei(device->playbackHandle, buf, frames);
        if( written < 0 )
        {
            result = Result(false, "Audio write failed %s", snd_strerror(written));
        }
        return result;
    }

    Result Drop(Device* device)
    {
        Result result;
        int err = snd_pcm_drop(device->playbackHandle);
        if( err < 0 )
        {
            result = Result(false, "Cannot prepare audio interface for use %s", snd_strerror(err));
            return result;
        }

        err = snd_pcm_prepare(device->playbackHandle);
        if( err < 0 )
        {
            result = Result(false, "Cannot prepare audio interface for use %s", snd_strerror(err));
            return result;
        }
        return result;
    }

    Result Pause(Device* device)
    {
        Result result;
        int err = snd_pcm_pause(device->playbackHandle,1);
        if( err < 0 )
        {
            result = Result(false, "Cannot prepare audio interface for use %s", snd_strerror(err));
            return result;
        }
        return result;
    }

    Result Resume(Device* device)
    {
        Result result;
        int err = snd_pcm_pause(device->playbackHandle,0);
        if( err < 0 )
        {
            result = Result(false, "Cannot prepare audio interface for use %s", snd_strerror(err));
            return result;
        }
        return result;
    }

    void Destroy(Device* device)
    {
        snd_pcm_close(device->playbackHandle);
        delete device;
        
    }

}

