#pragma once

#include "PointsHolder.hpp"
#include <functional>

struct Grid : public PointsHolder {
    void build(int n, int m, int width, int height);

    void apply(std::function<Color(Vec2)> &&point_to_color);
};

