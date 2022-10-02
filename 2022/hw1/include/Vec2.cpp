#include "Vec2.h"
#include <algorithm>

Vec2::Vec2(float x, float y) : x(x), y(y) {}

float Vec2::square_len() const {
    return x * x + y * y;
}

void Vec2::move90() {
    std::swap(x, y);
    x *= -1.f;
}

Vec2 operator+(Vec2 a, Vec2 b) {
    return Vec2{a.x + b.x, a.y + b.y};
}

Vec2 operator-(Vec2 a, Vec2 b) {
    return Vec2{a.x - b.x, a.y - b.y};
}

Vec2 operator*(float sc, Vec2 a) {
    return {a.x * sc, a.y * sc};
}
