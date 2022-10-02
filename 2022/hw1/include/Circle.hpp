#pragma once

#include "Vec2.h"
#include "util.hpp"

struct Circle {
    Vec2 center, direction;
    float radius;
    float r, g, b;

    Circle(Vec2 center, Vec2 direction, float radius, float r, float g, float b);

    [[nodiscard]] float f(Vec2 pos) const;

    void move(float time, float width, float height);
};