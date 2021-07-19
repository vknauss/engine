#ifndef RENDER_DEBUG_H_INCLUDED
#define RENDER_DEBUG_H_INCLUDED

#define VKRENDER_DEBUG
#ifdef VKRENDER_DEBUG
#include <iostream>
#include <sstream>

#define VKR_DEBUG_PRINT(X) { std::stringstream sstr; sstr << X << std::endl; std::cout << sstr.str(); std::flush(std::cout); }
#define VKR_DEBUG_CHECK_ERROR(X) X; VKR_Debug::checkError(#X);

//#define VKRENDER_DEBUG_PRINT_RUNTIME_ENABLED
#ifdef VKRENDER_DEBUG_PRINT_RUNTIME_ENABLED
#include "core/util/timer.h"
#define VKR_DEBUG_PRINT_RUNTIME(X) { uint64_t ticks = Timer::getTimerValue(); X; VKR_DEBUG_PRINT("Call " << #X << " runtime: " << (1000. * (Timer::getTimerValue()-ticks)/Timer::getTimerFrequency()) << " ms") }
#else
#define VKR_DEBUG_PRINT_RUNTIME(X) X;
#endif // VKRENDER_DEBUG_PRINT_RUNTIME_ENABLED

#define VKR_DEBUG_CALL(X) VKR_DEBUG_PRINT_RUNTIME(X); VKR_Debug::checkError(#X);

#else

#define VKR_DEBUG_PRINT(X)
#define VKR_DEBUG_CHECK_ERROR(X) X;
#define VKR_DEBUG_CALL(X) X;

#endif // VKRENDER_DEBUG

namespace VKR_Debug {

void checkError(const std::string& callString);

}

#endif // RENDER_DEBUG_H_INCLUDED
