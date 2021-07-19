#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include <cstdint>

class Timer {

    Timer() = delete;

    static uint64_t frameCounter;

public:

    static uint64_t getCurrentFrame() {
        return frameCounter;
    }

    static uint64_t getTimerValue();

    static uint64_t getTimerFrequency();

    static double getTime();

    static void incrementFrame() {
        ++frameCounter;
    }

};
#endif // TIMER_H_INCLUDED
