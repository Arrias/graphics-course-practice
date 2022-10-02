#pragma once

#include <string>
#include <chrono>
#include <random>

#include "Vec2.h"

const int GridWidth = 800;
const int GridHeight = 600;
const int WindowWidth = 800;
const int WindowHeight = 600;
const int GridQuantityDelta = 10;

const float eps = 0.0001;
const std::string program_name = "Crazy functions";

const auto sq2 = (float) sqrt(2);
const std::vector<Vec2> directions = {
        {0.f,  1.f},
        {sq2,  sq2},
        {1.f,  0.f},
        {sq2,  -sq2},
        {0.f,  -1.f},
        {-sq2, -sq2},
        {-1.f, 0.f},
        {-sq2, sq2},
};
