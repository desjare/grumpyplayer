# grumpyplayer

Experimental linux video player video player based on glfw, OpenGL, ALSA, ffmpeg, boost and curl. It supports full screen mode using the 'f' key, drag & drop and bicubic gpu resizing. It is also possible to launch it with a URL and the video will streamed.

This is a proof of concept of how to design a player with modules having very little coupling together. It is written in C++ but with the Go language approach in mind. While the implementation uses encapsulation & structure, data definitions are public and accessible. It relies heavily on C++ standard library or boost but it uses a more a C style design approach in the implementation. PEP-20 states that explicit is better than implicit and beautiful is better than ugly. Those principles should apply to C++ as well. The player was written with that in mind.

The player uses a consumer and producer pattern to play and decode and fetch the media. It is multithreaded and uses lock free queue as IPC. It has no locks.

Main modules are:
* player
* gui (glfw3)
* audiodevice (ALSA)
* videodevice (OpenGL)
* mediadecoder (ffmpeg)

OpenGL implementation has been taken from from https://gist.github.com/rcolinray/7552384. It has been adapted for windows scaling and video resizing and slightly rewritten. The resizing GLSL bicubic algorithm shader is from http://www.java-gaming.org/index.php?topic=35123.0. It is not clear if the implementation is accurate.

The png library used is lodepng. It can be found here: https://lodev.org/lodepng/. The player uses picopng implementation to load its icon. The icon is from http://www.pngall.com/?p=23412 and is on Creative Commons 4.0 BY-NC License. It can be found here: https://creativecommons.org/licenses/by-nc/4.0/.

<h1>Requirements</h1>

 * ffmpeg libs version 4 or higher (ffmpeg --version)
 * ALSA asound dev lib
 * swsscale lib
 * glfw3 lib
 * curl lib
 * cmake
 
 <h2>Ubuntu 18.04</h2>
 To install ffmpeg 4:
 See: http://ubuntuhandbook.org/index.php/2018/10/install-ffmpeg-4-0-2-ubuntu-18-0416-04/

<h1>Build</h1>

* cmake CMakeLists.txt
* make

<h1>Bugs</h1>

* The player pre-buffer a lot of frames and is consuming a lot of memory.
* When streaming video, it will buffer the entire file and release bytes as they are read.
* The player windows implementation is incomplete. It has no sound. It is also possibly buggy and untested.
