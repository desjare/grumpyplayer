#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>

#include "mediadecoder.h"
#include "audiodevice.h"
#include "videodevice.h"
#include "player.h"
#include "gui.h"
#include "profiler.h"

#include "result.h"

namespace {
    void WindowSizeChangeCallback(gui::Handle* handle, uint32_t w, uint32_t h, videodevice::Device* device)
    {
        videodevice::SetSize(device, w,h);
    }
}


void Init()
{
    profiler::Init();

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

Result CreateDevices(videodevice::Device*& videoDevice, audiodevice::Device*& audioDevice, mediadecoder::Decoder* decoder)
{
    Result result;

    mediadecoder::VideoStream* videoStream = decoder->videoStream;
    mediadecoder::AudioStream* audioStream = decoder->audioStream;

    const uint32_t videoWidth = videoStream->width;
    const uint32_t videoHeight = videoStream->height;
    const uint32_t channels = audioStream->channels;
    const uint32_t sampleRate = audioStream->sampleRate;
    const SampleFormat sampleFormat = audioStream->sampleFormat;

    result = audiodevice::Create(audioDevice, channels, sampleRate, sampleFormat );
    if(!result)
    {
        result = Result(false, result.getError());
        return result;
    }

    result = videodevice::Create(videoDevice, videoWidth, videoHeight);
    if(!result)
    {
        result = Result(false, result.getError());
        return result;
    }
    return result;
}

void EnableProfiler(bool enable)
{
    profiler::Enable(enable);
}

int main(int argc, char** argv)
{
    std::string filename;

    Init();

    try{
		boost::program_options::options_description desc{"Options"};
		desc.add_options()
		  ("help,h", "Help screen")
		  ("filename", boost::program_options::value<std::string>())
		  ("profiler", boost::program_options::bool_switch()->default_value(false)->notifier(EnableProfiler));

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

    // gui 
    gui::Handle* uiHandle = NULL;

    // devices
    audiodevice::Device* audioDevice = NULL;
    videodevice::Device* videoDevice = NULL;

    // decoder
    mediadecoder::Decoder* decoder = NULL;

    // player
    player::Player* player = NULL;

    // create decoder and open media
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

    // create windows 
    mediadecoder::VideoStream* videoStream = decoder->videoStream;
    const uint32_t videoWidth = videoStream->width;
    const uint32_t videoHeight = videoStream->height;

    result = CreateWindows(uiHandle, videoWidth, videoHeight);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    // create audio and video devices
    result = CreateDevices(videoDevice, audioDevice, decoder);
    if(!result)
    {
        std::cerr << result.getError() << std::endl;
        return 1;
    }

    gui::WindowSizeChangeCb windowSizeChangeCallback 
                  = boost::bind(WindowSizeChangeCallback, _1, _2, _3, videoDevice );
    gui::SetWindowSizeChangeCallback(uiHandle, windowSizeChangeCallback);

    // initialize the player
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

    // start playback
    player::Play(player);

    while( !gui::ShouldClose(uiHandle) )
    {
        // wait for frame time and draw frame
        player::Present(player);

        // print profile point stats if enable
        profiler::Print();

        // poll ui events
        gui::PollEvents();
    }

    return 0;
}
