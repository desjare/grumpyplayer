
#pragma once

#include<vector>

enum SampleFormat
{
    SF_FMT_U8,
    SF_FMT_S16,
    SF_FMT_S32,
    SF_FMT_FLOAT,
    SF_FMT_DOUBLE,
    SF_FMT_INVALID
};

enum AudioChannel
{
    AC_CH_INVALID = 0,
    AC_CH_FRONT_LEFT,
    AC_CH_FRONT_RIGHT,
    AC_CH_FRONT_CENTER,
    AC_CH_LOW_FREQUENCY,
    AC_CH_BACK_LEFT,
    AC_CH_BACK_RIGHT,
    AC_CH_FRONT_LEFT_OF_CENTER,
    AC_CH_FRONT_RIGHT_OF_CENTER,
    AC_CH_BACK_CENTER,
    AC_CH_SIDE_LEFT,
    AC_CH_SIDE_RIGHT,
    AC_CH_TOP_CENTER,
    AC_CH_TOP_FRONT_LEFT,
    AC_CH_TOP_FRONT_CENTER,
    AC_CH_TOP_FRONT_RIGHT,
    AC_CH_TOP_BACK_LEFT,
    AC_CH_TOP_BACK_CENTER,
    AC_CH_TOP_BACK_RIGHT
};

enum VideoFormat
{
    VF_RGB24,
    VF_YUV420P,
    VF_INVALID
};

typedef std::vector<VideoFormat> VideoFormatList;
