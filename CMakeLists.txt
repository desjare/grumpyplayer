cmake_minimum_required(VERSION 3.10)
project(grumpyplayer)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Debug Build")
    add_definitions(-D_DEBUG)	
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

# Linux defs
if (UNIX)
    add_definitions(-DHAVE_ALSA)	
    add_definitions(-DUNIX)	
    add_compile_options(-msse -msse2 -msse3 -msse4 -msse5 -mavx -mavx2 -g)
    add_compile_options(-Wall)
    add_compile_options(-I./3rdparty)
    add_compile_options(-I/usr/include/freetype2)

    find_library(AVFORMAT_LIBRARY avformat REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(AVCODEC_LIBRARY avcodec REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(AVUTIL_LIBRARY avutil REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(SOUND_LIBRARY asound REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(GLFW_LIBRARY glfw REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(SWSCALE_LIBRARY swscale REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(SWRESAMPLE_LIBRARY swresample REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(CURL_LIBRARY curl REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(BOOST_FILESYSTEM_LIBRARY boost_filesystem REQUIRED HINTS /usr/lib/x86_64-linux-gnu)
    find_library(BOOST_SYSTEM_LIBRARY boost_system REQUIRED HINTS  /usr/lib/x86_64-linux-gnu)
    find_library(BOOST_REGEX_LIBRARY boost_regex REQUIRED HINTS  /usr/lib/x86_64-linux-gnu)
    find_library(FREETYPE_LIBRARY freetype REQUIRED HINTS  /usr/lib/x86_64-linux-gnu)

    set(THREAD_LIBRARY pthread)
    set(GL_LIBRARY GL GLU)

    SET(NET_LIBRARY )
    SET(RC )

endif (UNIX)

# Windows defs
if (WIN32)
    set(3RDPARTY ${CMAKE_SOURCE_DIR}/3rdparty)
    set(LIBARCH win64)

    find_library(AVFORMAT_LIBRARY avformat REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
    find_library(AVCODEC_LIBRARY avcodec REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
    find_library(AVUTIL_LIBRARY avutil REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
    find_library(ASOUND_LIBRARY asound REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
    find_library(SWSCALE_LIBRARY swscale REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
    find_library(SWRESAMPLE_LIBRARY swresample REQUIRED HINTS ${3RDPARTY}/ffmpeg/lib/${LIBARCH})
    find_library(GLFW_LIBRARY glfw3 REQUIRED HINTS ${3RDPARTY}/glfw/lib/${LIBARCH})
    find_library(CURL_LIBRARY curl REQUIRED HINTS ${3RDPARTY}/curl/lib/${LIBARCH})
	find_library(FREETYPE_LIBRARY freetype REQUIRED HINTS  ${3RDPARTY}/freetype/lib/${LIBARCH})

    # boost deps are automatically added
    link_directories( ${3RDPARTY}/boost/lib/${LIBARCH} )

    set(BOOST_PROGRAM_OPTIONS_LIBRARY )
    set(BOOST_FILESYSTEM_LIBRARY )
    set(BOOST_SYSTEM_LIBRARY )
    set(BOOST_REGEX_LIBRARY )

    set(SOUND_LIBRARY )
    set(THREAD_LIBRARY )
    set(GL_LIBRARY opengl32 Glu32)
    SET(NET_LIBRARY Ws2_32 Secur32 Bcrypt)
    SET(RC grumpyplayer.rc)

    add_compile_options(-I${3RDPARTY})
    add_compile_options(-I${3RDPARTY}/ffmpeg/include)
    add_compile_options(-I${3RDPARTY}/boost/include)
    add_compile_options(-I${3RDPARTY}/glfw/include)
    add_compile_options(-I${3RDPARTY}/curl/include)
    add_compile_options(-I${3RDPARTY}/glm/include)
	add_compile_options(-I${3RDPARTY}/freetype/include/)
	add_compile_options(-I${3RDPARTY}/freetype/include/freetype2/)
	add_compile_options(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

    add_definitions(-DCURL_STATICLIB)

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ignore:4099")

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
    subtitle
    3rdparty/lodepng/picopng
    ${RC})


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
    ${BOOST_PROGRAM_OPTIONS_LIBRARY}
    ${BOOST_SYSTEM_LIBRARY}
    ${BOOST_FILESYSTEM_LIBRARY}
    ${BOOST_REGEX_LIBRARY}
    ${NET_LIBRARY}
    ${THREAD_LIBRARY}
    ${GL_LIBRARY}
    ${FREETYPE_LIBRARY})


install(TARGETS grumpyplayer
    RUNTIME DESTINATION "/usr/bin"
    LIBRARY DESTINATION "/usr/bin"
    DESTINATION "/usr/bin/"
)

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "desjare@gmail.com")
SET(CPACK_PACKAGE_VERSION_MAJOR 0)
SET(CPACK_PACKAGE_VERSION_MINOR 0)
SET(CPACK_PACKAGE_VERSION_PATCH 1)
SET(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/desjare/grumpyplayer")
SET(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
SET(CPACK_DEBIAN_PACKAGE_SECTION video)

INCLUDE(CPack)
