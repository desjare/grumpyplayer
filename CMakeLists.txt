cmake_minimum_required(VERSION 3.10)
project(grumpy)

set(CMAKE_CXX_STANDARD 14)

add_compile_options(-msse -msse2 -msse3 -msse4 -msse5 -mavx -mavx2 -g)
add_compile_options(-Wno-deprecated -Wno-deprecated-declarations)

find_library(AVFORMAT_LIBRARY avformat HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVCODEC_LIBRARY avcodec HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVUTIL_LIBRARY avutil HINTS /usr/lib/x86_64-linux-gnu)
find_library(AVDEVICE_LIBRARY avdevice HINTS /usr/lib/x86_64-linux-gnu)
find_library(ASOUND_LIBRARY asound HINTS /usr/lib/x86_64-linux-gnu)
find_library(GLFW_LIBRARY glfw HINTS /usr/lib/x86_64-linux-gnu)
find_library(SWSCALE_LIBRARY swscale HINTS /usr/lib/x86_64-linux-gnu)

add_definitions( -D__AVX2__ )
add_executable(grumpyplayer main videodevice audiodevice gui mediadecoder player stats)

target_link_libraries(
	grumpyplayer 
	${AVFORMAT_LIBRARY} 
	${AVCODEC_LIBRARY} 
	${AVUTIL_LIBRARY}
	${AVDEVICE_LIBRARY}
    ${ASOUND_LIBRARY}
    ${GLFW_LIBRARY}
    ${SWSCALE_LIBRARY}
    boost_program_options
    pthread
    GL)
