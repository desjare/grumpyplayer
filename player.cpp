#include "precomp.h"
#include "player.h"
#include "chrono.h"
#include "profiler.h"
#include "logger.h"
#include "scopeguard.h"

#include <boost/bind.hpp>

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
                logger::Trace("No audio frame count:%d", nbNoFrame);
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
        Result result;
        if(!mediadecoder::GetHaveAudio(player->decoder))
        {
            return result;
        }

        StartAudioThread(player);

        result = audiodevice::StartWhenReady(player->audioDevice);
        if(!result)
        {
            return result;
        }

        return result;
    }

    void StopAudio(player::Player* player, bool drop)
    {
        if(!mediadecoder::GetHaveAudio(player->decoder))
        {
            return;
        }

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

    void PauseAudio(player::Player* player)
    {
        if(mediadecoder::GetHaveAudio(player->decoder))
        {
            audiodevice::Pause(player->audioDevice);
        }
    }

    void ResumeAudio(player::Player* player)
    {
        if(mediadecoder::GetHaveAudio(player->decoder))
        {
            audiodevice::Resume(player->audioDevice);
        }
    }

    void DrawSubtitle(player::Player* player)
    {
         if(!player->subtitle)
         {
             mediadecoder::Consume(player->producer, player->subtitle);
         }

         if(!player->nextSubtitle)
         {
             mediadecoder::Consume(player->producer, player->nextSubtitle);
         }

         // subtitle
         if(player->subtitle)
         {
             const int64_t subtitleDuration = player->subtitle->endTimeUs - player->subtitle->startTimeUs;
             int64_t subtitleWait = chrono::Wait(player->playbackStartTimeUs, player->subtitle->startTimeUs);
             
             const bool inSubtitleTime = (subtitleWait <= 0 && subtitleWait >= -subtitleDuration);

             logger::Info("subtitle wait %lf %s", chrono::Seconds(subtitleWait), player->subtitle->text[0].c_str());

             if(inSubtitleTime)
             {
                 mediadecoder::Subtitle* sub = player->subtitle;

                 uint32_t width = 0;
                 uint32_t height = 0;

                 videodevice::GetWindowSize(player->videoDevice, width, height);

                 // ass header
                 subtitle::GetTextSizeCb textSizeCb = boost::bind(videodevice::GetTextSize, player->videoDevice, _1, _2, _3, _4, _5);
                 subtitle::SubLineList lines;
                 
                 Result result = subtitle::GetDisplayInfo(sub->text, textSizeCb, width, height, sub->header, sub->dialogue, sub->startTimeUs, sub->endTimeUs, 
                                                           sub->fontName, sub->fontSize, sub->color, lines);

                 if(!result)
                 {
                    logger::Error("Error getting subtitle display info: %s", result.getError().c_str());
                    return;
                 }

                 for(auto it = lines.begin(); it != lines.end(); ++it)
                 {
                    videodevice::DrawText(player->videoDevice, (*it).text, sub->fontName, sub->fontSize, (*it).x, (*it).y, 1, sub->color);
                 }
             }

             if(subtitleWait < -subtitleDuration)
             {
                 mediadecoder::Release(player->producer, player->subtitle);

                 // go to next subtitle
                 player->subtitle = player->nextSubtitle;
                 player->nextSubtitle = nullptr;
             }

             // duration is not always provided and a default duration is set to subtitle
             // verify that the next subtitle should not be displaying
             if(player->nextSubtitle)
             {
                 const int64_t nextSubtitleDuration = player->nextSubtitle->endTimeUs - player->nextSubtitle->startTimeUs;
                 int64_t nextSubtitleWait = chrono::Wait(player->playbackStartTimeUs, player->nextSubtitle->startTimeUs);
                 
                 const bool inNextSubtitleTime = (nextSubtitleWait <= 0 && nextSubtitleWait >= -nextSubtitleDuration);
                 if(inNextSubtitleTime)
                 {
                     if(player->subtitle)
                     {
                         mediadecoder::Release(player->producer, player->subtitle);
                     }
                     player->subtitle = player->nextSubtitle;
                     player->nextSubtitle = nullptr;
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

    Result Create(Player*& player)
    {
        Result result;
        player = new Player();

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

        if(mediadecoder::GetHaveVideo(player->decoder))
        {
            result = videodevice::Create(player->videoDevice, mediadecoder::GetOutputFormat(player->decoder));
            if(!result)
            {
                result = Result(false, result.getError());
                return result;
            }
        }

        player->path = filename;

        result = mediadecoder::Create(player->producer, player->decoder);
        if(!result)
        {
            return result;
        }

        if(mediadecoder::GetHaveVideo(player->decoder))
        {
            result = videodevice::SetTextureSize(player->videoDevice, 
                                                 mediadecoder::GetVideoWidth(player->decoder), 
                                                 mediadecoder::GetVideoHeight(player->decoder));
            if(!result)
            {
                return result;
            }

            SetWindowSize(player, 
                          mediadecoder::GetVideoWidth(player->decoder),
                          mediadecoder::GetVideoHeight(player->decoder));
        }

        if(mediadecoder::GetHaveAudio(player->decoder))
        {
            const uint32_t channels = mediadecoder::GetAudioNumChannels(player->decoder);
            const uint32_t sampleRate = mediadecoder::GetAudioSampleRate(player->decoder);
            const SampleFormat sampleFormat = mediadecoder::GetAudioSampleFormat(player->decoder);

            result = audiodevice::SetInputFormat(player->audioDevice,channels,sampleRate,sampleFormat);
            if(!result)
            {
                return result;
            }
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

    void ToggleSubtitleTrack(Player* player)
    {
        if(player && player->decoder)
        {
            mediadecoder::ToggleSubtitleTrack(player->decoder);
        }
    }

    void AddSubtitleTrack(Player* player, std::shared_ptr<subtitle::SubRip> srt)
    {
        if(player && player->decoder)
        {
            mediadecoder::AddSubtitleTrack(player->decoder, srt);
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
            ResumeAudio(player);
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

        PauseAudio(player);
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

                 // draw subtitle
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
