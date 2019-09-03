
#include "precomp.h"
#include "audiodevice.h"
#include "logger.h"

#include <system_error>
#include <thread>

namespace {

#ifdef HAVE_ALSA
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

    snd_pcm_chmap_position AudioChannelToASound(AudioChannel ch)
    {
        switch(ch)
        {
            case AC_CH_FRONT_LEFT:
                return SND_CHMAP_FL;
            case AC_CH_FRONT_RIGHT:
                return SND_CHMAP_FR;
            case AC_CH_FRONT_CENTER:
                return SND_CHMAP_FC;
            case AC_CH_LOW_FREQUENCY:
                return SND_CHMAP_LFE;
            case AC_CH_BACK_LEFT:
                return SND_CHMAP_RL;
            case AC_CH_BACK_RIGHT:
                return SND_CHMAP_RR;
            case AC_CH_FRONT_LEFT_OF_CENTER:
                return SND_CHMAP_FLC;
            case AC_CH_FRONT_RIGHT_OF_CENTER:
                return SND_CHMAP_FRC;
            case AC_CH_BACK_CENTER:
                return SND_CHMAP_RC;
            case AC_CH_SIDE_LEFT:
                return SND_CHMAP_SL;
            case AC_CH_SIDE_RIGHT:
                return SND_CHMAP_SR;
            case AC_CH_TOP_CENTER:
                return SND_CHMAP_TC;
            case AC_CH_TOP_FRONT_LEFT:
                return SND_CHMAP_TFL;
            case AC_CH_TOP_FRONT_CENTER:
                return SND_CHMAP_TFC;
            case AC_CH_TOP_FRONT_RIGHT:
                return SND_CHMAP_TFR;
            case AC_CH_TOP_BACK_LEFT:
                return SND_CHMAP_TRL;
            case AC_CH_TOP_BACK_CENTER:
                return SND_CHMAP_TRC;
            case AC_CH_TOP_BACK_RIGHT:
                return SND_CHMAP_TRR;
            default:
                break;
        }
        return SND_CHMAP_UNKNOWN;
    }
#endif

}

namespace audiodevice
{
#ifdef HAVE_ALSA
    Result Create(Device*& device)
    {
        Result result;

        device = new Device();

        int err = snd_pcm_open(&device->playbackHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if( err < 0 )
        {
            result = Result(false, "Cannot open audio device %s", snd_strerror(err));
            return result;
        }
        return result;
    }

    void Destroy(Device*& device)
    {
        snd_pcm_close(device->playbackHandle);
        delete device;
        device = NULL;
    }

    Result SetInputFormat(Device* device, uint32_t channels, uint32_t sampleRate, SampleFormat sampleFormat, const std::vector<AudioChannel>& mapping)
    {
        Result result;

        assert(device);
        if(!device)
        {
            result = Result(false, "Invalid audiodevice");
        }

        snd_pcm_hw_params_t* hwParams = NULL;
        int err = snd_pcm_hw_params_malloc(&hwParams);
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

        err = snd_pcm_hw_params_set_format (device->playbackHandle, hwParams,SampleFormatToASound(sampleFormat));
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

        // chmap
        size_t size = offsetof(snd_pcm_chmap_t, pos[channels]);
        assert(size >= sizeof(snd_pcm_chmap_t));

        snd_pcm_chmap_t* map = (snd_pcm_chmap_t*) calloc(1, size);
        map->channels = channels;
   
        for (uint32_t i = 0 ; i < map->channels ; ++i) 
        {
            map->pos[i] = AudioChannelToASound(mapping[i]);
        }

        err = snd_pcm_set_chmap(device->playbackHandle, map);
        if(err < 0 )
        {
            // apparently a known pulse alsa issue
            logger::Error("Cannot set chmap %s", snd_strerror(err));
        }

        free(map);

        err = snd_pcm_hw_params_can_pause(hwParams);
        if( err != 1 ) // can pause
        {
            result = Result(false, "Cannot prepare audio interface for use since it cannot pause.");
            return result;
        }

        return result;

    }

    Result StartWhenReady(Device* device)
    {
        Result result;
        while( snd_pcm_state(device->playbackHandle) != SND_PCM_STATE_RUNNING )
        {
            std::this_thread::yield();
        }
        return result;
    }

    Result WriteInterleaved(Device* device, void* buf, uint32_t frames, std::atomic<bool>& bufferInUse)
    {
        Result result;

        assert(device);
        if(!device)
        {
            return Result(false, "Invalid audiodevice");
        }
        bufferInUse = false;

        snd_pcm_sframes_t written = snd_pcm_writei(device->playbackHandle, buf, frames);
        if( written < 0 )
        {
            snd_pcm_prepare(device->playbackHandle);
            result = Result(false, "Audio write failed %s", snd_strerror(written));
        }
        return result;
    }

    Result Flush(Device* device)
    {
        Result result;

        assert(device);
        if(!device)
        {
            result = Result(false, "Invalid audiodevice");
        }

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

        assert(device);
        if(!device)
        {
            result = Result(false, "Invalid audiodevice");
        }

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

        assert(device);
        if(!device)
        {
            result = Result(false, "Invalid audiodevice");
        }

        int err = snd_pcm_pause(device->playbackHandle,0);
        if( err < 0 )
        {
            result = Result(false, "Cannot prepare audio interface for use %s", snd_strerror(err));
            return result;
        }
        return result;
    }

#endif

#ifdef WIN32
    class Audio2VoiceCallback : public IXAudio2VoiceCallback
    {
    public:
        Audio2VoiceCallback() {}
        ~Audio2VoiceCallback() {}

        //Called when the voice has just finished playing a contiguous audio stream.
        void OnStreamEnd() { }
        void OnVoiceProcessingPassEnd() { }
        void OnVoiceProcessingPassStart(UINT32 SamplesRequired) { }
        
        void OnBufferEnd(void* pBufferContext) 
        {
            std::atomic<bool>* bufferInUse = reinterpret_cast<std::atomic<bool>*>(pBufferContext);
            *bufferInUse = false;
        }

        void OnBufferStart(void* pBufferContext) {    }
        void OnLoopEnd(void* pBufferContext) {    }
        void OnVoiceError(void* pBufferContext, HRESULT Error) { }
    };

    Result Create(Device*& device)
    {
        Result result;

        HRESULT hr;
        if (FAILED(hr = CoInitialize(NULL)))
        {
            return Result(false, "CoInitialize failed. Error: %s", std::system_category().message(hr).c_str());
        }

        device = new Device();
        memset(device, 0, sizeof(Device));

        if (FAILED(hr = XAudio2Create(&device->xaudioHandle, 0, XAUDIO2_DEFAULT_PROCESSOR)))
        {
            return Result(false, "XAudio2Create failed. Error: %s", std::system_category().message(hr).c_str());
        }

        return result;
    }

    Result SetInputFormat(Device* device, uint32_t channels, uint32_t sampleRate, SampleFormat sampleFormat, const std::vector<AudioChannel>& mapping)
    {
        Result result;

        HRESULT hr;
        if (FAILED(hr = device->xaudioHandle->CreateMasteringVoice(&device->masterVoice, channels, sampleRate, 0, 0, 0)))
        {
            return Result(false, "CreateMasteringVoice failed. Error: %s", std::system_category().message(hr).c_str());
        }

        WAVEFORMATEX& wfx = device->wfx;
        wfx.nChannels = channels;
        wfx.nSamplesPerSec = sampleRate;

        switch (sampleFormat)
        {
        case SF_FMT_U8:
            wfx.wBitsPerSample = 8;
            wfx.wFormatTag = WAVE_FORMAT_PCM;
            break;
        case SF_FMT_S16:
            wfx.wBitsPerSample = 16;
            wfx.wFormatTag = WAVE_FORMAT_PCM;
            break;
        case SF_FMT_S32:
            wfx.wBitsPerSample = 32;
            wfx.wFormatTag = WAVE_FORMAT_PCM;
            break;
        case SF_FMT_FLOAT:
            wfx.wBitsPerSample = 32;
            wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            break;
        case SF_FMT_DOUBLE:
            wfx.wBitsPerSample = 64;        
            wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            break;
        case SF_FMT_INVALID:
            return Result(false, "Invalid Sample Format.");
        }

        wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        device->voiceCallbacks = new Audio2VoiceCallback();

        if (FAILED(hr = device->xaudioHandle->CreateSourceVoice(&device->sourceVoice, (WAVEFORMATEX*)&wfx, 0, 2.0f, device->voiceCallbacks)))
        {
            return Result(false, "CreateSourceVoice failed. Error: %s", std::system_category().message(hr).c_str());
        }

        return result;
    }

    Result WriteInterleaved(Device* device, void* buf, uint32_t nbSamples, std::atomic<bool>& bufferInUse)
    {
        const uint32_t sleepTimeOnErrorMMS = 100;
        Result result;
        
        const WAVEFORMATEX& wfx = device->wfx;
        XAUDIO2_BUFFER buffer;
        memset(&buffer, 0, sizeof(XAUDIO2_BUFFER));
        
        buffer.AudioBytes = nbSamples * wfx.nChannels * wfx.wBitsPerSample / 8;
        buffer.pAudioData = reinterpret_cast<BYTE*>(buf);
        buffer.pContext = &bufferInUse;

        bufferInUse = true;

        HRESULT hr;

        do
        {
            if (FAILED(hr = device->sourceVoice->SubmitSourceBuffer(&buffer)) && hr != XAUDIO2_E_INVALID_CALL)
            {
                return Result(false, "SubmitSourceBuffer failed. Error: %s", std::system_category().message(hr).c_str());
            }

            if (FAILED(hr))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeOnErrorMMS));
            }

        } while (hr == XAUDIO2_E_INVALID_CALL);


        return result;
    }

    Result StartWhenReady(Device* device)
    {
        const uint32_t REQUIRED_BUFFERS = 5;
        Result result;

        HRESULT hr;
        
        XAUDIO2_VOICE_STATE state;

        do
        {
            device->sourceVoice->GetState(&state, 0);

        } while (state.BuffersQueued < REQUIRED_BUFFERS);
    

        if (FAILED(hr = device->sourceVoice->Start()))
        {
            return Result(false, "Start failed. Error: %s", std::system_category().message(hr).c_str());
        }
        
        return result;
    }

    Result Pause(Device* device)
    {
        Result result;
        HRESULT hr;
        if (FAILED(hr = device->sourceVoice->Stop()))
        {
            return Result(false, "Stop failed. Error: %s", std::system_category().message(hr).c_str());
        }
        return result;
    }

    Result Resume(Device* device)
    {
        Result result;
        HRESULT hr;
        if (FAILED(hr = device->sourceVoice->Start()))
        {
            return Result(false, "Start failed. Error: %s", std::system_category().message(hr).c_str());
        }
        return result;
    }

    Result Flush(Device* device)
    {
        if (!device->sourceVoice)
        {
            return Result(false, "audiodevice::Drop Cannot drop sinnce audio is not configure yet.");
        }

        Result result;
        HRESULT hr;
        if (FAILED(hr = device->sourceVoice->FlushSourceBuffers()))
        {
            return Result(false, "FlushSourceBuffers failed. Error: %s", std::system_category().message(hr).c_str());
        }

        return result;
    }

    void Destroy(Device*& device)
    {
        if (device)
        {
            device->xaudioHandle->Release();
            delete device->voiceCallbacks;

            delete device;
            device = NULL;
        }
    }
#endif

}

