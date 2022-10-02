#pragma once

#include <Color.h>
#include <Vec2.h>
#include <util.hpp>

struct PointsHolder {
    std::vector<Vec2> points;
    std::vector<Color> colors;
    std::vector<uint32_t> ids;
    GLuint points_vbo{}, colors_vbo{}, vao{}, ebo{};

    PointsHolder();

    void updColors() const;

    void updPoints() const;

    void updIndexes() const;

    [[nodiscard]] size_t size() const;
};
