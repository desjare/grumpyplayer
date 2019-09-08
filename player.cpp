#include "precomp.h"
#include "player.h"
#include "chrono.h"
#include "profiler.h"
#include "logger.h"
#include "scopeguard.h"

namespace {
    const int64_t queueFullSleepTimeMs = 100;
    const int64_t pauseSleepTimeMs = 500;
    const int64_t doneSleepTimeMs = 2000;

    player::SwapBufferCallback swapBufferCallback;
}

namespace {
    bool WaitForPlayback(const char* name, uint64_t startTimeUs, uint64_t timeUs, int64_t waitThresholdUs)
    {
        const int64_t sleepThresholdUs = 10000;
        const int64_t millisecondUs = 1000;
        const int64_t logDeltaThresholdUs = 100;

        int64_t waitTime = chrono::Wait(startTimeUs, timeUs);

        if( waitTime <= 0 )
        {
            logger::Warn("WaitForPlayback. %s Late Playback. %f seconds", name, chrono::Seconds(waitTime));
            return true;
        }
        else
        {
            if( waitTime >= sleepThresholdUs )
            {
                std::this_thread::sleep_for(std::chrono::microseconds(waitTime-millisecondUs));
                waitTime = chrono::Wait(startTimeUs, timeUs);

                if( waitTime <= 0 )
                {
                    return true;
                }
            }

            if( waitTime > 0 )
            {
                do
                {
                    std::this_thread::yield();
                    waitTime = chrono::Wait(startTimeUs, timeUs);

                } while( waitTime >= waitThresholdUs );
            } 

            const uint64_t currentTimeUs = chrono::Current(startTimeUs);
            const int64_t deltaUs = std::abs( static_cast<int64_t>(currentTimeUs-timeUs) );

            if( deltaUs > logDeltaThresholdUs )
            {
                logger::Warn("WaitForPlayback %s play time %ld us decode %ld us diff %ld\n", name, currentTimeUs, timeUs, deltaUs );
            }
        }
        return true;
    }

    void WaitSeekEnd(player::Player* player)
    {
        while( mediadecoder::IsSeeking(player->producer) )
        {
            std::this_thread::yield();
        }
    }

    void AudioPlaybackThread(player::Player* player)
    {
        mediadecoder::AudioFrame* audioFrame = nullptr;
        Result result;
        uint32_t nbNoFrame = 0;
  
        while(player->queueAudio)
        {
            if(!audioFrame)
            {
                mediadecoder::Consume(player->producer, audioFrame);
            }

            if( audioFrame )
            {
                nbNoFrame = 0;

                int64_t waitTime 
                           = chrono::Wait(player->playbackStartTimeUs, audioFrame->timeUs);

                if( !player->buffering && waitTime >= audiodevice::ENQUEUE_SAMPLES_US)
                {
                    logger::Debug("AudioPlayThread sleeping. Wait Time %f", chrono::Seconds(waitTime) );
                    std::this_thread::sleep_for(std::chrono::milliseconds(queueFullSleepTimeMs));
                    continue;
                }
               
                profiler::ScopeProfiler profiler(profiler::PROFILER_AUDIO_WRITE);
                result = audiodevice::WriteInterleaved( player->audioDevice, audioFrame->samples, audioFrame->nbSamples, audioFrame->inUse );
                if(!result)
                {
                    logger::Error("AudioDeviceWriteInterleaved failed %s", result.getError().c_str());
                }
                mediadecoder::Release(player->producer, audioFrame);
                audioFrame = nullptr;
            }
            else
            {
                nbNoFrame += 1;
                logger::Debug("No audio frame count:%d", nbNoFrame);
            }
        }
    }

    void StartAudioThread(player::Player* player)
    {
        if(player->queueAudio == false)
        {
            player->queueAudio = true;
            player->audioThread = std::thread(AudioPlaybackThread, player);
        }
    }

    Result StartAudioPlayback(player::Player* player)
    {
        StartAudioThread(player);

        Result result = audiodevice::StartWhenReady(player->audioDevice);
        if(!result)
        {
            return result;
        }

        return result;
    }

    void StopAudio(player::Player* player, bool drop)
    {
        player->queueAudio = false;

        if( player->audioThread.joinable() )
        {
            player->audioThread.join();
        }

        if( drop )
        {
            audiodevice::Flush(player->audioDevice);
        }
    }

    void DrawSubtitle(player::Player* player)
    {
         // subtitle
         if(player->subtitle)
         {
             const int64_t subtitleDuration = player->subtitle->endTimeUs - player->subtitle->startTimeUs;
             int64_t subtitleWait = chrono::Wait(player->playbackStartTimeUs, player->subtitle->startTimeUs);
             
             const bool inSubtitleTime = (subtitleWait <= 0 && subtitleWait >= -subtitleDuration);

             if(inSubtitleTime)
             {
                 float tw = 0.0f;
                 float th = 0.0f;
                 uint32_t w = 0;
                 uint32_t h = 0;
                 
                 int fontSize = 48;

                 videodevice::GetTextSize(player->videoDevice, player->subtitle->text, fontSize, tw, th);
                 videodevice::GetWindowSize(player->videoDevice, w, h);

                 float x = w / 2.0f - tw / 2.0f;

                 videodevice::DrawText(player->videoDevice, player->subtitle->text, fontSize, x, 50, 1, glm::vec3(1,1,1));
             }

             if(subtitleWait < -subtitleDuration)
             {
                 mediadecoder::Release(player->producer, player->subtitle);
                 player->subtitle = nullptr;
             }
         }
    }
}

namespace player
{
    Result Init(SwapBufferCallback cb)
    {
        Result result;
        swapBufferCallback = cb;
        return result;
    }

    Result Create(Player*& player)
    {
        Result result;
        player = new Player();
        player->decoder = nullptr;
        player->producer = nullptr;
        player->videoFrame = nullptr;
        player->audioDevice = nullptr;
        player->videoDevice = nullptr ;
        player->playbackStartTimeUs = 0;
        player->currentTimeUs = 0;
        player->playing = false;
        player->pause = false;
        player->buffering = false;
        player->queueAudio = false;

        result = audiodevice::Create(player->audioDevice);
        if(!result)
        {
            result = Result(false, result.getError());
            return result;
        }

        return result;
    }

    Result Open(Player* player, const std::string& filename)
    {
        Close(player);

        Result result = mediadecoder::Create(player->decoder);
        if(!result)
        {
            return result;
        }
        result = mediadecoder::Open(player->decoder, filename);
        if(!result)
        {
            return result;
        }

        result = videodevice::Create(player->videoDevice, mediadecoder::GetOutputFormat(player->decoder));
        if(!result)
        {
            result = Result(false, result.getError());
            return result;
        }

        player->path = filename;

        result = mediadecoder::Create(player->producer, player->decoder);
        if(!result)
        {
            return result;
        }

        result = videodevice::SetTextureSize(player->videoDevice, player->decoder->videoStream->width, player->decoder->videoStream->height);
        if(!result)
        {
            return result;
        }

        SetWindowSize(player, player->decoder->videoStream->width, player->decoder->videoStream->height);

        mediadecoder::AudioStream* audioStream = player->decoder->audioStream;
        const uint32_t channels = audioStream->channels;
        const uint32_t sampleRate = audioStream->sampleRate;
        const SampleFormat sampleFormat = audioStream->sampleFormat;

        result = audiodevice::SetInputFormat(player->audioDevice,channels,sampleRate,sampleFormat);
        if(!result)
        {
            return result;
        }

        return result;
    }

    void SetWindowSize(Player* player, uint32_t w, uint32_t h)
    {
        assert(player);
        if(player && player->videoDevice)
        {
            videodevice::SetWindowSize(player->videoDevice, w, h);
        }
    }

    void ToggleSubtitle(Player* player)
    {
        if(player && player->decoder)
        {
            mediadecoder::ToggleSubtitle(player->decoder);
        }
    }

    void Play(Player* player)
    {
        logger::Info("Play");

        if(player->playing)
        {
            logger::Warn("Player Play called while playing");
            return;
        }

        if(!player->decoder || !player->producer)
        {
            logger::Warn("Player Play called without media");
            return;
        }

        // start buffering
        assert(!player->buffering);

        scopeguard::SetValue bufferingGuard(player->buffering, true, false);

        mediadecoder::WaitForPlayback(player->producer);

        if(!player->pause)
        {
            Result result = StartAudioPlayback(player);
            if(!result)
            {
                logger::Error("Cannot start audio %s", result.getError().c_str());
                return;
            }
        }
        else
        {
            audiodevice::Resume(player->audioDevice);
            player->pause = false;
        }

        // start playback
        player->playbackStartTimeUs = chrono::Now() - player->currentTimeUs;
        player->playing = true;
    }

    void Seek(Player* player, uint64_t timeUs)
    {
        if( mediadecoder::IsSeeking(player->producer) )
        {
            logger::Warn("Player seek failed. Already seeking.");
            return;
        }

        logger::Info("Seek start at %f ms", chrono::Milliseconds(timeUs));
        mediadecoder::Seek(player->producer, timeUs);

        // stop audio playback
        StopAudio(player, true);

        // wait for seek
        WaitSeekEnd(player);

        // start buffering
        assert(!player->buffering);
        scopeguard::SetValue bufferingGuard(player->buffering, true, false);

        Result result = StartAudioPlayback(player);
        if(!result)
        {
            logger::Error("Cannot start audio %s", result.getError().c_str());
            return;
        }

        // reset playback start time
        player->playbackStartTimeUs = static_cast<int64_t>(chrono::Now()) - static_cast<int64_t>(timeUs);
        
        logger::Info("Seek end");
    }

    void Pause(Player* player)
    {
        logger::Info("Pause");
        if(!player->playing)
        {
            return;
        }
        player->playing = false;
        player->pause = true;
        audiodevice::Pause(player->audioDevice);
    }

    bool IsPlaying(Player* player)
    {
        return player->playing;
    }

    uint64_t GetDuration(Player* player)
    {
        if(!player || !player->decoder)
        {
            return 0;
        }

        return mediadecoder::GetDuration(player->decoder);
    }

    uint64_t GetCurrentTime(Player* player)
    {
        if(!player)
        {
            return 0;
        }

        return player->currentTimeUs;
    }

    const std::string& GetPath(Player* player)
    {
        assert(player);
        return player->path;
    }

    void Present(Player* player)
    {
         if(!player->playing)
         {
             std::this_thread::sleep_for(std::chrono::milliseconds(pauseSleepTimeMs));
             return;
         }

         if(!player->videoFrame)
         {
             mediadecoder::Consume(player->producer, player->videoFrame);
         }

         if(!player->subtitle)
         {
             mediadecoder::Consume(player->producer, player->subtitle);
         }

         if( player->videoFrame  )
         {
             player->currentTimeUs = player->videoFrame->timeUs;
             const uint64_t waitThresholdUs = 25;
             const bool drawFrame 
                       = WaitForPlayback("video", player->playbackStartTimeUs, player->videoFrame->timeUs, waitThresholdUs);

             if( drawFrame )
             {
                 profiler::ScopeProfiler profiler(profiler::PROFILER_VIDEO_DRAW);

                 // create fb based on VideoFrame
                 videodevice::FrameBuffer fb;
                 fb.width = player->videoFrame->width;
                 fb.height = player->videoFrame->height;
                 for(uint32_t i = 0; i < videodevice::NUM_FRAME_DATA_POINTERS; i++)
                 {
                     fb.frameData[i] = player->videoFrame->buffers[i];
                     fb.lineSize[i] = player->videoFrame->lineSize[i];
                 }

                 // draw frame
                 videodevice::DrawFrame(player->videoDevice, &fb);

                 DrawSubtitle(player);

                 // swap buffer
                 swapBufferCallback();

                 mediadecoder::Release(player->producer, player->videoFrame);
                 player->videoFrame = nullptr;
             }
             else 
             {
                 logger::Warn( "Player: No draw frame" );
             }
         }
         else if(player->producer->done)
         {
             std::this_thread::sleep_for(std::chrono::milliseconds(doneSleepTimeMs));
         }
         else 
         {
             logger::Warn( "Player: No video frame" );
         } 
    }

    void Close(Player* player)
    {
        StopAudio(player, true);

        player->playbackStartTimeUs = 0;
        player->currentTimeUs = 0;
        player->playing = false;
        player->pause = false;

        mediadecoder::Destroy(player->producer);
        mediadecoder::Destroy(player->decoder);

        videodevice::Destroy(player->videoDevice);
    }

    void Destroy(Player*& player)
    {
        Close(player);

        audiodevice::Destroy(player->audioDevice);

        delete player;
        player = nullptr;
    }
}
