
#include "player.h"
#include "chrono.h"
#include "profiler.h"
#include "logger.h"

namespace {
    const int64_t queueFullSleepTimeMs = 500;
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
            logger::Warn("WaitForPlayback. %s Late Playback. %f\n", name, chrono::Seconds(waitTime));
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
        mediadecoder::AudioFrame* audioFrame = NULL;
        Result result;
  
        while(player->queueAudio)
        {
            if(!audioFrame)
            {
                mediadecoder::Consume(player->producer, audioFrame);
            }

            if( audioFrame )
            {
                int64_t waitTime 
                           = chrono::Wait(player->playbackStartTimeUs, audioFrame->timeUs);

                if( waitTime >= audiodevice::ENQUEUE_SAMPLES_US )
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(queueFullSleepTimeMs));
                    continue;
                }
               
                profiler::ScopeProfiler profiler(profiler::PROFILER_AUDIO_WRITE);
                result = audiodevice::WriteInterleaved( player->audioDevice, audioFrame->samples, audioFrame->nbSamples );
                if(!result)
                {
                    logger::Error("AudioDeviceWriteInterleaved failed %s", result.getError().c_str());
                }
                mediadecoder::Release(player->producer, audioFrame);
                audioFrame = NULL;
            }
        }
    }

    void StartAudio(player::Player* player)
    {
        player->queueAudio = true;
        player->audioThread = std::thread(AudioPlaybackThread, player);
    }

    void StopAudio(player::Player* player, bool drop)
    {
        player->queueAudio = false;
        player->audioThread.join();
        if( drop )
        {
            audiodevice::Drop(player->audioDevice);
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

    Result Create(Player*& player, mediadecoder::Decoder* decoder, audiodevice::Device* audioDevice, videodevice::Device* videoDevice)
    {
        Result result;
        player = new Player();
        player->decoder = decoder;
        player->videoFrame = NULL;
        player->audioDevice = audioDevice;
        player->videoDevice = videoDevice;
        player->playbackStartTimeUs = 0;
        player->queueAudio = false;

        result = mediadecoder::Create(player->producer, decoder);
        if(!result)
        {
            return result;
        }
        return result;
    }

    void Play(Player* player)
    {
        mediadecoder::WaitForPlayback(player->producer);

        player->playbackStartTimeUs = chrono::Now();

        StartAudio(player);
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

        result = mediadecoder::Create(player->producer, player->decoder);
        if(!result)
        {
            return result;
        }

        result = videodevice::SetVideoSize(player->videoDevice, player->decoder->videoStream->width, player->decoder->videoStream->height);
        if(!result)
        {
            return result;
        }

        Play(player); 

        return result;
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

        // start audio playback
        StartAudio(player);

        // reset playback start time
        player->playbackStartTimeUs = static_cast<int64_t>(chrono::Now()) - static_cast<int64_t>(timeUs);
        logger::Info("Seek end");
    }

    uint64_t GetDuration(Player* player)
    {
        if(!player || !player->decoder)
        {
            return 0;
        }

        return mediadecoder::GetDuration(player->decoder);
    }

    void Present(Player* player)
    {
         const uint32_t videoWidth = player->decoder->videoStream->width;
         const uint32_t videoHeight = player->decoder->videoStream->height;

         if(!player->videoFrame)
         {
             mediadecoder::Consume(player->producer, player->videoFrame);
         }

         if( player->videoFrame  )
         {
             const uint64_t waitThresholdUs = 25;
             const bool drawFrame 
                       = WaitForPlayback("video", player->playbackStartTimeUs, player->videoFrame->timeUs, waitThresholdUs);

             if( drawFrame )
             {
                 profiler::ScopeProfiler profiler(profiler::PROFILER_VIDEO_DRAW);

                 videodevice::DrawFrame(player->videoDevice, player->videoFrame->frame, videoWidth, videoHeight);
                 swapBufferCallback();

                 mediadecoder::Release(player->producer, player->videoFrame);
                 player->videoFrame = NULL;
             }
             else if( !player->videoFrame )
             {
                 logger::Warn( "Player: No video frame" );
             } 
             else 
             {
                 logger::Warn( "Player: No draw frame" );
             }
         }
    }

    void Close(Player* player)
    {
        StopAudio(player, true);

        player->playbackStartTimeUs = 0;

        mediadecoder::Destroy(player->producer);
        player->producer = NULL;

        mediadecoder::Destroy(player->decoder);
        player->decoder = NULL;
    }

    void Destroy(Player* player)
    {
        Close(player);
        delete player;
    }
}
