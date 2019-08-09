#pragma once

#ifdef WIN32

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef ERROR
#undef min
#undef max
#undef GetCurrentTime

#endif
