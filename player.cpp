
#include "player.h"
#include "playertime.h"

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
            uint64_t audiopopStartTime = playertime::Now();
            if(!audioFrame)
            {
                if( player->producer->audioQueue->pop(audioFrame) )
                {
                    player->producer->audioQueueSize--;
                }
            }
             if( audioFrame )
             {
                 const bool writeFrame = true;
                         //  = mediadecoder::producer::WaitForPlayback("audio", player->playbackStartTimeUs, audioFrame->timeUs);

                 if( writeFrame )
                 {
                     uint64_t startInterTime = playertime::Now();
                     result = audiodevice::WriteInterleaved( player->audioDevice, audioFrame->samples, audioFrame->nbSamples );
                     uint64_t endInterTime = playertime::Now();
                     if(!result)
                     {
                         std::cerr << "AudioDeviceWriteInterleaved failed " << result.getError() << std::endl;
                     }
                     mediadecoder::producer::Destroy(player->producer, audioFrame);
                     audioFrame = NULL;
                     uint64_t audiopopEndTime = playertime::Now();
                     //std::cerr << "Audio Time " << audiopopEndTime - audiopopStartTime << "write audio " << endInterTime - startInterTime << std::endl;
                 }
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

        player->playbackStartTimeUs = playertime::Now();
        player->play = true;
        player->audioThread = std::thread(AudioPlaybackThread, player);
    }

    void Present(Player* player)
    {
         const uint32_t videoWidth = player->decoder->videoStream->width;
         const uint32_t videoHeight = player->decoder->videoStream->height;

         uint64_t popStartTime = playertime::Now();
         if(!player->videoFrame)
         {
             if( player->producer->videoQueue->pop(player->videoFrame) )
             {
                 player->producer->videoQueueSize--;
             }
         }

         if( player->videoFrame  )
         {
             const bool drawFrame 
                       = playertime::WaitForPlayback("video", player->playbackStartTimeUs, player->videoFrame->timeUs);

             if( drawFrame )
             {
                 uint64_t drawStartTime = playertime::Now();

                 videodevice::DrawFrame(player->videoDevice, player->videoFrame->frame, videoWidth, videoHeight);

                 uint64_t drawFrameEndTime = playertime::Now();

                 swapBufferCallback();

                 uint64_t drawEndTime = playertime::Now();

                 std::cerr << "Draw Time Pop " << drawStartTime - popStartTime << " DrawFrame " << drawFrameEndTime - drawStartTime << " Swap " << drawEndTime - drawFrameEndTime << std::endl;

                 mediadecoder::producer::Destroy(player->producer, player->videoFrame);
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
