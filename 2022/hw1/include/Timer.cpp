#include "Timer.hpp"


Timer::Timer() : last_frame_start(std::chrono::high_resolution_clock::now()), time(0.f) {}

float Timer::tick() {
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration_cast<std::chrono::duration<float >>(now - last_frame_start).count();
    last_frame_start = now;
    time += dt;
    return dt;
}
