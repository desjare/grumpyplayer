#include "precomp.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <string>
#include <iostream>

#include "mediadecoder.h"
#include "audiodevice.h"
#include "videodevice.h"
#include "player.h"
#include "gui.h"
#include "profiler.h"
#include "logger.h"
#include "chrono.h"

#include "result.h"

namespace {
    void WindowSizeChangeCallback(gui::Handle* handle, uint32_t w, uint32_t h, videodevice::Device* device)
    {
        videodevice::SetWindowSize(device, w,h);
    }

    void FileDropCallback(gui::Handle* handle, const std::string& filename, player::Player* player)
    {
        Result result = player::Open(player, filename);
        if(!result)
        {
            logger::Error("Unable to open %s: %s", filename.c_str(), result.getError().c_str());
        }
        else
        {
            gui::SetWindowSize(handle, player->decoder->videoStream->width, player->decoder->videoStream->height);
        }
    }

    void SeekCallback(gui::Handle* handle, double percent, player::Player* player)
    {
        const uint64_t duration = player::GetDuration(player);
        const double pos = static_cast<double>(duration) * percent;

        logger::Debug("SeekCallback duration %f s percent %f %% pos %f s" , chrono::Seconds(duration), percent, chrono::Seconds(static_cast<uint64_t>(pos)));
        player::Seek(player, static_cast<uint64_t>(pos));
    }

    void PauseCallback(gui::Handle* handle, player::Player* player)
    {
        if( player::IsPlaying(player) )
        {
            player::Pause(player);
        }
        else
        {
            player::Play(player);
        }
    }

    void SetWindowTitle(gui::Handle* handle, uint64_t timeUs, uint64_t duration, const std::string& program, const std::string& filename)
    {
        boost::filesystem::path path(filename);

        std::string title = program + std::string(" - ") + path.filename().string() + 
                            std::string(" - ") + chrono::HoursMinutesSeconds(timeUs) + 
                            "/" + chrono::HoursMinutesSeconds(duration);

        gui::SetTitle( handle, title.c_str() );
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
        logger::Error(result.getError().c_str());
        return result;
    }
    result = gui::Create(ui);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return result;
    }

    result = gui::OpenWindow(ui, videoWidth, videoHeight);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return result;
    }
    return result;
}

Result CreateDevices(videodevice::Device*& videoDevice, audiodevice::Device*& audioDevice, mediadecoder::Decoder* decoder)
{
    Result result;

    mediadecoder::AudioStream* audioStream = decoder->audioStream;

    const uint32_t channels = audioStream->channels;
    const uint32_t sampleRate = audioStream->sampleRate;
    const SampleFormat sampleFormat = audioStream->sampleFormat;

    result = audiodevice::Create(audioDevice, channels, sampleRate, sampleFormat );
    if(!result)
    {
        result = Result(false, result.getError());
        return result;
    }

    result = videodevice::Create(videoDevice);
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

void SetLogLevel(std::string level)
{
    logger::Level logLevel = logger::GetLevelFromString(level);
    if( logLevel == logger::INVALID )
    {
        logger::Error("Invalid log level %s", level.c_str() );
        exit(1);
    }

    logger::SetLevel(logLevel);
}

int main(int argc, char** argv)
{
    std::string program = boost::filesystem::path(argv[0]).filename().string();
    std::string filename;

    Init();

    try{
		boost::program_options::options_description desc{"Options"};
		desc.add_options()
		  ("help,h", "Print program options.")
		  ("filename", boost::program_options::value<std::string>(), "Path the the media file.")
		  ("profiler", boost::program_options::bool_switch()->default_value(false)->notifier(EnableProfiler), "Enable profiling.")
		  ("loglevel", boost::program_options::value<std::string>(), "Specify log level: debug, info, warning or error.");

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);
        
        if( vm.count("help" ) )
        {
            std::cout << desc;
            return 1;
        }

        if( vm.count("filename") )
        {
            filename = vm["filename"].as<std::string>();
        }
        else
        {
            logger::Error("No filename spacified.");
            std::cerr << desc;
            return 1;
        }

        if( vm.count("loglevel") )
        {
            SetLogLevel(vm["loglevel"].as<std::string>());
        }
    }
    catch( boost::program_options::error& ex)
    {
        logger::Error("Program Option Error: %s", ex.what());
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
        logger::Error(result.getError().c_str());
        return 1;
    }
    result = mediadecoder::Open(decoder, filename);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    // create windows 
    mediadecoder::VideoStream* videoStream = decoder->videoStream;
    const uint32_t videoWidth = videoStream->width;
    const uint32_t videoHeight = videoStream->height;

    result = CreateWindows(uiHandle, videoWidth, videoHeight);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    // create audio and video devices
    result = CreateDevices(videoDevice, audioDevice, decoder);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    videodevice::SetVideoSize(videoDevice, videoWidth, videoHeight);

    // initialize the player
    player::SwapBufferCallback swapBufferCallback 
                     = boost::bind( gui::SwapBuffers, uiHandle );

    result = player::Init( swapBufferCallback );
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    result = player::Create(player, decoder, audioDevice, videoDevice);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    // initialize gui callbacks
    gui::WindowSizeChangeCb windowSizeChangeCallback 
                  = boost::bind(WindowSizeChangeCallback, _1, _2, _3, videoDevice );

    gui::FileDropCb fileDropCallback
                  = boost::bind(FileDropCallback, _1, _2, player);

    gui::SeekCb seekCallback
                  = boost::bind(SeekCallback, _1, _2, player);

    gui::PauseCb pauseCallback
                  = boost::bind(PauseCallback, _1, player);

    gui::SetWindowSizeChangeCallback(uiHandle, windowSizeChangeCallback);
    gui::SetFileDropCallback(uiHandle, fileDropCallback);
    gui::SetSeekCallback(uiHandle, seekCallback);
    gui::SetPauseCallback(uiHandle, pauseCallback);


    // start playback
    player::Play(player);

    while( !gui::ShouldClose(uiHandle) )
    {
        // wait for frame time and draw frame
        player::Present(player);

        // print profile point stats if enable
        profiler::Print();

        SetWindowTitle(uiHandle, player::GetCurrentTime(player), 
                       player::GetDuration(player), program, filename);

        // poll ui events
        gui::PollEvents();
    }

    player::Destroy(player);

    videodevice::Destroy(videoDevice);
    audiodevice::Destroy(audioDevice);

    gui::Destroy();

    return 0;
}
