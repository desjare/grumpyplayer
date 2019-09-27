# grumpyplayer

Experimental linux & windows video player video player based on glfw, OpenGL, ALSA, XAudio2, ffmpeg, boost and curl. It supports full screen mode (double click), drag & drop, bicubic gpu resizing & ssa/ass and srt subtitles. It is also possible to launch it with a URL and the video will be streamed.

This is a proof of concept of how to design a player with modules having very little coupling together. It is written in C++ but with the Go language approach in mind. While the implementation uses encapsulation & structure, data definitions are public and accessible. It relies heavily on C++ standard library or boost but it uses a more a C style design approach in the implementation and APIs. PEP-20 states that explicit is better than implicit and beautiful is better than ugly. These principles should apply to C++ as well and I often see overly complicated class hirarchies that are difficult to maintain. The player was written with that in mind.

The player uses a consumer and producer pattern to play and decode and fetch the media. It is multithreaded and uses lock free queue as IPC. It has no locks.

Main modules are:
* player
* audiodevice (ALSA or XAudio2)
* videodevice (OpenGL)
* mediadecoder (ffmpeg)
* gui (glfw3)
* streamer (curl)
* subtitle (ssa/ass,srt)

Overall I think it is a good example of how to use ffmpeg to decode a video from file or stream and use the video and audio media for playback.

<h2>Credits</h2>

I used and inspired myself of several projects writing the player so here are some credits. Sometimes I took things as his but most of the time I modified them to add some features or rewritten them partially. Licensing of all those projects are sometimes unclear.

* Original RGB shader has been largely inspired from from from https://gist.github.com/rcolinray/7552384.
* The resizing GLSL bicubic algorithm shader is from http://www.java-gaming.org/index.php?topic=35123.0. However, it is not clear if the implementation is accurate.
* H264 YUV420P -> RGB Shader is from https://gist.github.com/roxlu/9329339. The shaders were not modified but I refactored slightly the their player code to integrate it to the rendering code and support windows and texture resizing.
* The png library used is lodepng. It can be found here: https://lodev.org/lodepng/. The player uses picopng implementation to load its icon. 
* The icon is from http://www.pngall.com/?p=23412 and is on Creative Commons 4.0 BY-NC License. It can be found here: https://creativecommons.org/licenses/by-nc/4.0/.

<h1>Requirements</h1>

 * ffmpeg libs version 4 or higher (ffmpeg --version)
 * ALSA asound dev lib (linux)
 * swsscale lib
 * glfw3 lib
 * curl lib
 * freetype2 lib
 * boost libs
 * cmake
 
 <h2>Ubuntu 18.04</h2>
 
 To install ffmpeg 4:
 See: http://ubuntuhandbook.org/index.php/2018/10/install-ffmpeg-4-0-2-ubuntu-18-0416-04/

<h2>Ubuntu 19.04</h2>

Install dependencies:

sudo apt install libavcodec-dev libavformat-dev libcurl4-openssl-dev libasound2-dev libboost-all-dev libglfw3-dev libfreetype6-dev libswscale-dev libglm-dev libglu1-mesa-dev


 
 <h2>Windows</h2>
 
All 3rdparty libs are included in 3rdparty folder fox x64 architecture. Binary files are on git-lfs. It links statically with everything

<h1>Build</h1>

* cmake CMakeLists.txt
* make

<h1>Bugs</h1>

* Below two audio channels, the audio channel mapping has not been tested. It is currently configured according to pulse and XAudio2 configuration but no tests has been done beside the stereo mix. The player is able to play 5.1 audio on stereo on linux but has not been tested beyond that.
* The player pre-buffer a lot of frames and is consuming a lot of memory.
