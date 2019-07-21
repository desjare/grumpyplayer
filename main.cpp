#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>

#include "mediadecoder.h"
#include "audiodevice.h"
#include "videodevice.h"
#include "player.h"
#include "gui.h"

#include "result.h"


void Init()
{
    mediadecoder::Init();
    videodevice::Init();
}

Result CreateWindows(gui::Handle*& ui, int videoWidth, int videoHeight)
{
    Result result = gui::Init();
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return result;
    }
    result = gui::Create(ui);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return result;
    }

    result = gui::OpenWindow(ui, videoWidth, videoHeight);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return result;
    }
    return result;
}

int main(int argc, char** argv)
{
    std::string filename;

    Init();

    try{
		boost::program_options::options_description desc{"Options"};
		desc.add_options()
		  ("help,h", "Help screen")
		  ("filename", boost::program_options::value<std::string>());

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);

        if( vm.count("filename") )
        {
            filename = vm["filename"].as<std::string>();
        }

    }
    catch( boost::program_options::error& ex)
    {
        std::cerr  << ex.what() << "\n";
    }

    if(filename.empty())
    {
        return 1;
    }

    gui::Handle* uiHandle = NULL;

    audiodevice::Device* audioDevice = NULL;
    videodevice::Device* videoDevice = NULL;

    mediadecoder::Decoder* decoder = NULL;
    mediadecoder::producer::Producer* producer = NULL;
    player::Player* player = NULL;

    Result result = mediadecoder::Create(decoder);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }
    result = mediadecoder::Open(decoder, filename);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    mediadecoder::VideoStream* videoStream = decoder->videoStream;
    mediadecoder::AudioStream* audioStream = decoder->audioStream;

    const uint32_t videoWidth = videoStream->width;
    const uint32_t videoHeight = videoStream->height;
    const uint32_t channels = audioStream->channels;
    const uint32_t sampleRate = audioStream->sampleRate;
    const SampleFormat sampleFormat = audioStream->sampleFormat;

    result = CreateWindows(uiHandle, videoWidth, videoHeight);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    result = audiodevice::Create(audioDevice, channels, sampleRate, sampleFormat );
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    result = videodevice::Create(videoDevice, videoWidth, videoHeight);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;

        return 1;
    }

    player::SwapBufferCallback swapBufferCallback = boost::bind( gui::SwapBuffers, uiHandle );

    result = player::Init( swapBufferCallback );
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    result = player::Create(player, decoder, audioDevice, videoDevice);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    
    player::Play(player);

    while( !gui::ShouldClose(uiHandle) )
    {
        player::Present(player);
        gui::PollEvents();
    }

/*
    mediadecoder::producer::WaitForPlayback(producer);

    uint64_t playbackStartTimeUs = mediadecoder::producer::GetEpochTimeUs();

    mediadecoder::producer::VideoFrame* videoFrame = NULL;
    mediadecoder::producer::AudioFrame* audioFrame = NULL;

    while( gui::ShouldClose(ui) )
    {
         uint64_t popStartTime = mediadecoder::producer::GetEpochTimeUs();
         if(!videoFrame)
         {
             if( producer->videoQueue->pop(videoFrame) )
             {
                 producer->videoQueueSize--;
             }
         }

         if( videoFrame  )
         {
             const bool drawFrame 
                       = mediadecoder::producer::WaitForPlayback("video", playbackStartTimeUs, videoFrame->timeUs);

             if( drawFrame )
             {
                 uint64_t drawStartTime = mediadecoder::producer::GetEpochTimeUs();
                 videodevice::DrawFrame(videoDevice, videoFrame->frame, videoWidth, videoHeight);
                 uint64_t drawFrameEndTime = mediadecoder::producer::GetEpochTimeUs();
                 gui::SwapBuffers(ui);
                 uint64_t drawEndTime = mediadecoder::producer::GetEpochTimeUs();

                 std::cerr << "Draw Time Pop " << drawStartTime - popStartTime << " DrawFrame " << drawFrameEndTime - drawStartTime << " Swap " << drawEndTime - drawFrameEndTime << std::endl;

                 mediadecoder::producer::Destroy(producer, videoFrame);
                 videoFrame = NULL;
             }
             else if( !videoFrame )
             {
                 std::cerr << "No video frame." << std::endl;
             }
         }
         
         uint64_t audiopopStartTime = mediadecoder::producer::GetEpochTimeUs();
         if(!audioFrame)
         {
             if( producer->audioQueue->pop(audioFrame) )
             {
                 producer->audioQueueSize--;
             }
         }
         if( audioFrame )
         {
             const bool writeFrame 
                       = mediadecoder::producer::WaitForPlayback("audio", playbackStartTimeUs, audioFrame->timeUs);

             if( writeFrame )
             {
                 uint64_t startInterTime = mediadecoder::producer::GetEpochTimeUs();
                 result = audiodevice::WriteInterleaved( audioDevice, audioFrame->samples, audioFrame->nbSamples );
                 uint64_t endInterTime = mediadecoder::producer::GetEpochTimeUs();
                 if(!result)
                 {
                     std::cerr << "AudioDeviceWriteInterleaved failed " << result.getError() << std::endl;
                 }
                 mediadecoder::producer::Destroy(producer, audioFrame);
                 audioFrame = NULL;
                 uint64_t audiopopEndTime = mediadecoder::producer::GetEpochTimeUs();
                 std::cerr << "Audio Time " << audiopopEndTime - audiopopStartTime << "write audio " << endInterTime - startInterTime << std::endl;
             }
         }
    }
    */

    return 0;
}
