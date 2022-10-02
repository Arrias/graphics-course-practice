#include "Circle.hpp"

Circle::Circle(Vec2 center, Vec2 direction, float radius, float r, float g, float b) :
        center(center), direction(direction), radius(radius), r(r), g(g), b(b) {}

float Circle::f(Vec2 pos) const {
    return sqr(radius) / (pos - center).square_len();
}

void Circle::move(float time, float width, float height) {
    center = center + time * direction;
    if (center.x <= 0 || center.y <= 0 || center.x >= width || center.y >= height) {
        center = center - 2 * time * direction;
        direction.move90();
        //direction.move90();
    }
}
