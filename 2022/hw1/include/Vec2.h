#pragma once

struct Vec2 {
    float x{}, y{};

    Vec2(float x, float y);

    [[nodiscard]] float square_len() const;

    void move90();
};

Vec2 operator+(Vec2 a, Vec2 b);

Vec2 operator-(Vec2 a, Vec2 b);

Vec2 operator*(float sc, Vec2 a);