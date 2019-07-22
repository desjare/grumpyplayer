
#include "player.h"
#include "chrono.h"

#include <iostream>

namespace {
    player::SwapBufferCallback swapBufferCallback;
}

namespace {

    void AudioPlaybackThread(player::Player* player)
    {
        mediadecoder::producer::AudioFrame* audioFrame = NULL;
        Result result;
  
        while(player->play)
        {
            uint64_t audiopopStartTime = chrono::Now();
            if(!audioFrame)
            {
                mediadecoder::producer::Consume(player->producer, audioFrame);
            }
             if( audioFrame )
             {
                 uint64_t startInterTime = chrono::Now();
                 result = audiodevice::WriteInterleaved( player->audioDevice, audioFrame->samples, audioFrame->nbSamples );
                 uint64_t endInterTime = chrono::Now();
                 if(!result)
                 {
                     std::cerr << "AudioDeviceWriteInterleaved failed " << result.getError() << std::endl;
                 }
                 mediadecoder::producer::Release(player->producer, audioFrame);
                 audioFrame = NULL;
                 uint64_t audiopopEndTime = chrono::Now();
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

         uint64_t popStartTime = chrono::Now();
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
                 uint64_t drawStartTime = chrono::Now();

                 videodevice::DrawFrame(player->videoDevice, player->videoFrame->frame, videoWidth, videoHeight);

                 uint64_t drawFrameEndTime = chrono::Now();

                 swapBufferCallback();

                 uint64_t drawEndTime = chrono::Now();

                 std::cerr << "Draw Time Pop " << drawStartTime - popStartTime << " DrawFrame " << drawFrameEndTime - drawStartTime << " Swap " << drawEndTime - drawFrameEndTime << std::endl;

                 mediadecoder::producer::Release(player->producer, player->videoFrame);
                 player->videoFrame = NULL;
             }
             else if( !player->videoFrame )
             {
                 std::cerr << "No video frame." << std::endl;
             } else {
                 std::cerr << "No draw frame." << std::endl;
             }
         }
    }

}
