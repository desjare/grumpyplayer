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
