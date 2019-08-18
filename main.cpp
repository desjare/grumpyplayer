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
    void WindowSizeChangeCallback(gui::Handle* handle, uint32_t w, uint32_t h, player::Player* player)
    {
        player::SetWindowSize(player, w,h);
    }

    void FileDropCallback(gui::Handle* handle, const std::string& filename, player::Player* player)
    {
        Result result = player::Open(player, filename);
        if(result)
        {
            if( !gui::IsFullScreen(handle) )
            {
                gui::SetWindowSize(handle, player->decoder->videoStream->width, player->decoder->videoStream->height);
            }
            player::Play(player);
        }
        else
        {
            logger::Error("Unable to open %s: %s", filename.c_str(), result.getError().c_str());
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

    VideoFormatList supportedFormat;

    videodevice::GetSupportedFormat(supportedFormat);
    mediadecoder::SetOutputFormat(supportedFormat);
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

#ifdef WIN32

#include <shellapi.h>

char* WCharToUTF8(LPWSTR pWSTR)
{ 
    size_t wsize = wcslen(pWSTR);
    size_t sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, pWSTR, static_cast<int>(wsize), NULL, 0, NULL, NULL);
    char* str = new char[sizeNeeded+1];
    memset(str, 0, sizeNeeded + 1);
    WideCharToMultiByte(CP_UTF8, 0, pWSTR, static_cast<int>(wsize), str, static_cast<int>(sizeNeeded+1), NULL, NULL);
    return str;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const size_t cmdLineSize = strlen(lpCmdLine);

    int32_t argc = 0;
    size_t convertedSize = 0;
    wchar_t cmdLineW[BUFSIZ];

    // convert command line to wide string
    mbstowcs_s(&convertedSize, cmdLineW, cmdLineSize +1, lpCmdLine, _TRUNCATE);

    // convert windows command line to main style command line
    LPWSTR* argwv = CommandLineToArgvW(cmdLineW, &argc);

    // argv[0] is used for command line name
    char** argv = new char*[++argc];
    argv[0] = "grumpyplayer";

    for (int32_t i = 1; i < argc; i++)
    {
        argv[i] = WCharToUTF8(argwv[i-1]);
    }

#else
int main(int argc, char** argv)
{
#endif
    std::string program = "grumpyplayer";
    std::string path;

    Init();

    try{
        boost::program_options::positional_options_description p;
        p.add("path", -1);

        boost::program_options::options_description desc{"Options"};
        desc.add_options()
          ("help,h", "Print program options.")
          ("path", boost::program_options::value<std::string>(), "Path the the media file.")
          ("profiler", boost::program_options::bool_switch()->default_value(false)->notifier(EnableProfiler), "Enable profiling.")
          ("loglevel", boost::program_options::value<std::string>(), "Specify log level: debug, info, warning or error.");

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        boost::program_options::notify(vm);
        
        if( vm.count("help" ) )
        {
            std::cout << desc;
            return 1;
        }

        if( vm.count("path") )
        {
            path = vm["path"].as<std::string>();
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

    // player
    player::Player* player = NULL;

    // Create windowsw
    Result result = CreateWindows(uiHandle, 640, 480);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    // initialize the player
    player::SwapBufferCallback swapBufferCallback 
                     = boost::bind( gui::SwapBuffers, uiHandle );

    result = player::Init( swapBufferCallback );
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    result = player::Create(player);
    if(!result)
    {
        logger::Error(result.getError().c_str());
        return 1;
    }

    // open media
    if(!path.empty())
    {
        result = player::Open(player, path);
        if(!result)
        {
            logger::Error("Unable to open %s: %s", path.c_str(), result.getError().c_str());
            return 1;
        }

        // resize window to media size
        gui::SetWindowSize(uiHandle, player->decoder->videoStream->width, player->decoder->videoStream->height);
    }
    else
    {
        const uint32_t defaultWidth = 640;
        const uint32_t defaultHeight = 480;
        gui::SetWindowSize(uiHandle, defaultWidth, defaultHeight);
    }

    gui::ShowWindow(uiHandle);

    // initialize gui callbacks
    gui::WindowSizeChangeCb windowSizeChangeCallback 
                  = boost::bind(WindowSizeChangeCallback, _1, _2, _3, player );

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
                       player::GetDuration(player), program, player::GetPath(player));

        // poll ui events
        gui::PollEvents();
    }

    player::Destroy(player);
    gui::Destroy();

    return 0;
}
