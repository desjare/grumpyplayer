cmake_minimum_required(VERSION 3.10)
project(grumpy)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-msse -msse2 -msse3 -msse4 -msse5 -mavx -mavx2 -g)
add_compile_options(-Wall -Wno-deprecated -Wno-deprecated-declarations)
add_compile_options(-I./3rdparty)

find_library(AVFORMAT_LIBRARY avformat.58 REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVCODEC_LIBRARY avcodec.58 REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVUTIL_LIBRARY avutil.56 REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(ASOUND_LIBRARY asound REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(GLFW_LIBRARY glfw.3 REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
find_library(SWSCALE_LIBRARY swscale REQUIRED HINTS /usr/lib/x86_64-linux-gnu)

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
    3rdparty/lodepng/picopng)

target_link_libraries(
	grumpyplayer 
	${AVFORMAT_LIBRARY} 
	${AVCODEC_LIBRARY} 
	${AVUTIL_LIBRARY}
    ${ASOUND_LIBRARY}
    ${GLFW_LIBRARY}
    ${SWSCALE_LIBRARY}
    boost_program_options
    pthread
    GL)
