#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>

#include "mediadecoder.h"
#include "audiodevice.h"
#include "videodevice.h"
#include "player.h"
#include "gui.h"
#include "stats.h"

#include "result.h"


void Init()
{
    mediadecoder::Init();
    videodevice::Init();
    stats::Init();
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

void WindowSizeChangeCallback(gui::Handle* handle, uint32_t w, uint32_t h, videodevice::Device* device)
{
    videodevice::SetSize(device, w,h);
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

    gui::WindowSizeChangeCb windowSizeChangeCallback 
                  = boost::bind(WindowSizeChangeCallback, _1, _2, _3, videoDevice );
    gui::SetWindowSizeChangeCallback(uiHandle, windowSizeChangeCallback);

    player::SwapBufferCallback swapBufferCallback 
                     = boost::bind( gui::SwapBuffers, uiHandle );

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
        stats::PrintPoints();
        gui::PollEvents();
    }

    return 0;
}
