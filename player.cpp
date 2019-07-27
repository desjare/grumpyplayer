
#include "player.h"
#include "chrono.h"
#include "profiler.h"
#include "logger.h"

namespace {
    const int64_t queueFullSleepTimeMs = 500;
    player::SwapBufferCallback swapBufferCallback;
}

namespace {

    void AudioPlaybackThread(player::Player* player)
    {
        mediadecoder::producer::AudioFrame* audioFrame = NULL;
        Result result;
  
        while(player->play)
        {
            if(!audioFrame)
            {
                mediadecoder::producer::Consume(player->producer, audioFrame);
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
                mediadecoder::producer::Release(player->producer, audioFrame);
                audioFrame = NULL;
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

    Result Create(Player*& player, mediadecoder::Decoder* decoder, audiodevice::Device* audioDevice, videodevice::Device* videoDevice)
    {
        Result result;
        player = new Player();
        player->decoder = decoder;
        player->videoFrame = NULL;
        player->audioDevice = audioDevice;
        player->videoDevice = videoDevice;
        player->playbackStartTimeUs = 0;

        result = mediadecoder::producer::Create(player->producer, decoder);
        if(!result)
        {
            return result;
        }
        return result;
    }

    void Play(Player* player)
    {
        mediadecoder::producer::WaitForPlayback(player->producer);

        player->playbackStartTimeUs = chrono::Now();
        player->play = true;
        player->audioThread = std::thread(AudioPlaybackThread, player);
    }

    void Present(Player* player)
    {
         const uint32_t videoWidth = player->decoder->videoStream->width;
         const uint32_t videoHeight = player->decoder->videoStream->height;

         if(!player->videoFrame)
         {
             mediadecoder::producer::Consume(player->producer, player->videoFrame);
         }

         if( player->videoFrame  )
         {
             const uint64_t waitThresholdUs = 25;
             const bool drawFrame 
                       = chrono::WaitForPlayback("video", player->playbackStartTimeUs, player->videoFrame->timeUs, waitThresholdUs);

             if( drawFrame )
             {
                 profiler::ScopeProfiler profiler(profiler::PROFILER_VIDEO_DRAW);

                 videodevice::DrawFrame(player->videoDevice, player->videoFrame->frame, videoWidth, videoHeight);
                 swapBufferCallback();

                 mediadecoder::producer::Release(player->producer, player->videoFrame);
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

}
