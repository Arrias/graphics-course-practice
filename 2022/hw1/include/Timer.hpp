#pragma once

#include <chrono>

struct Timer {
    std::chrono::time_point<std::chrono::high_resolution_clock> last_frame_start;
    float time;

    Timer();

    float tick();
};