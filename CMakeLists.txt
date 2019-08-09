cmake_minimum_required(VERSION 3.10)
project(grumpyplayer)

set(CMAKE_CXX_STANDARD 17)


# Linux defs
if (UNIX)
add_definitions( -DHAVE_ALSA )	
add_definitions( -DUNIX )	
add_compile_options(-msse -msse2 -msse3 -msse4 -msse5 -mavx -mavx2 -g)
add_compile_options(-Wall -Wno-deprecated -Wno-deprecated-declarations)
add_compile_options(-I./3rdparty)

find_library(AVFORMAT_LIBRARY avformat REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVCODEC_LIBRARY avcodec REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVUTIL_LIBRARY avutil REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(SOUND_LIBRARY asound REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(GLFW_LIBRARY glfw REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(SWSCALE_LIBRARY swscale REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(SWRESAMPLE_LIBRARY swresample REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(CURL_LIBRARY curl REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(BOOST_PROGRAM_OPTION_LIBRARY boost_program_options REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(BOOST_FILESYSTEM_LIBRARY boost_filesystem REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(BOOST_SYSTEM_LIBRARY boost_system REQUIRED HINTS  /usr/lib/x86_64-linux-gnu)

set(THREAD_LIBRARY pthread)
set(GL_LIBRARY GL)
SET(NET_LIBRARY )

endif (UNIX)

# Windows defs
if (WIN32)

set(3RDPARTY ${CMAKE_SOURCE_DIR}/3rdparty)
set(LIBARCH win64)

link_directories( ${3RDPARTY}/boost/lib/${LIBARCH} )

find_library(AVFORMAT_LIBRARY avformat REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
find_library(AVCODEC_LIBRARY avcodec REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
find_library(AVUTIL_LIBRARY avutil REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
find_library(ASOUND_LIBRARY asound REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
find_library(SWSCALE_LIBRARY swscale REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
find_library(SWRESAMPLE_LIBRARY swresample REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})

find_library(GLFW_LIBRARY glfw3 REQUIRED HINTS ${3RDPARTY}/glfw/lib/${LIBARCH})
find_library(CURL_LIBRARY curl REQUIRED HINTS ${3RDPARTY}/curl/lib/${LIBARCH})
find_library(BOOST_PROGRAM_OPTION_LIBRARY boost_program_option REQUIRED HINTS ${3RDPARTY}/boost/lib/${LIBARCH})
find_library(BOOST_FILESYSTEM_LIBRARY boost_filesystem REQUIRED HINTS ${3RDPARTY}/boost/lib/${LIBARCH})
find_library(BOOST_SYSTEM_LIBRARY boost_system REQUIRED HINTS ${3RDPARTY}/boost/lib/${LIBARCH})


set(SOUND_LIBRARY "")
set(THREAD_LIBRARY "")
set(GL_LIBRARY opengl32)
SET(NET_LIBRARY Ws2_32 Secur32 Bcrypt)

add_compile_options(-I${3RDPARTY})
add_compile_options(-I${3RDPARTY}/ffmpeg/include)
add_compile_options(-I${3RDPARTY}/boost/include)
add_compile_options(-I${3RDPARTY}/glfw/include)
add_compile_options(-I${3RDPARTY}/curl/include)
add_compile_options(-I${3RDPARTY}/glm/include)
add_definitions( -DCURL_STATICLIB )

endif (WIN32)


add_definitions( -D__AVX2__ )
add_executable(grumpyplayer 
    main 
    videodevice 
    audiodevice 
    gui 
    mediadecoder 
    player 
    profiler
    logger
    icon 
    curl
    3rdparty/lodepng/picopng)


target_link_libraries(
    grumpyplayer 
    ${AVFORMAT_LIBRARY} 
    ${AVCODEC_LIBRARY} 
    ${AVUTIL_LIBRARY}
    ${SOUND_LIBRARY}
    ${GLFW_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ${CURL_LIBRARY}
    ${BOOST_PROGRAM_OPTION_LIBRARY}
    ${BOOST_SYSTEM_LIBRARY}
    ${BOOST_FILESYSTEM_LIBRARY}
    ${NET_LIBRARY}
    ${THREAD_LIBRARY}
    ${GL_LIBRARY})
