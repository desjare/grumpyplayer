# grumpyplayer

Experimental linux video player video player based on glfw, OpenGL, ALSA and ffmpeg. The player uses a consumer and producer pattern to play and decode and fetch the media. It is multithreaded and uses lock free queue as IPC. It has no locks.

Modules are:
* player
* gui
* audiodevice (ALSA)
* videodevice (OpenGL & glfw3)
* mediadecoder

This is a proof of concept of how to design a player with modules having very little coupling together. It is written in C++ but with the Go language approach in mind. It does not use polymorphism or exception.

OpenGL implementation has been adapted from https://gist.github.com/rcolinray/7552384. 

<h1>Requirements</h1>
 * ffmpeg libs version 4 or higher (ffmpeg --version)
 * ALSA asound dev lib
 * swsscale lib
 * glfw3 lib
 
 <h2>Ubuntu 18.04 </h2>
 To install ffmpeg 4:
 See: http://ubuntuhandbook.org/index.php/2018/10/install-ffmpeg-4-0-2-ubuntu-18-0416-04/

<h1>Build</h1>
  * cmake CMakeLists.txt
  * make
