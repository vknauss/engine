#include "timer.h"

#include <GLFW/glfw3.h>

uint64_t Timer::frameCounter = 0;

uint64_t Timer::getTimerValue() {
    return glfwGetTimerValue();
}

uint64_t Timer::getTimerFrequency() {
    return glfwGetTimerFrequency();
}

double Timer::getTime() {
    return glfwGetTime();
}

