#pragma once

#include "PointsHolder.hpp"
#include <functional>

struct Graph {
    int n, m;
    std::vector<std::vector<float>> dst;
    std::vector<std::vector<int>> h_points, v_points;
    PointsHolder grid, lines;

    Graph(int nn, int mm, int width, int height);

    [[nodiscard]] int get_id(int i, int j) const;

    void buildGrid(int nn, int mm, int width, int height);

    void apply(std::function<float(Vec2, Color &)> &&point_to_color);

    void addLine(float C = 1.0);

    void draw() const;
};

