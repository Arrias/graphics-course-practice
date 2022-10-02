#include "Grid.hpp"

void Grid::build(int n, int m, int width, int height) {
    points.clear();
    ids.clear();

    float shift_x = (float) width / (float) (n - 1);
    float shift_y = (float) height / (float) (m - 1);

    auto get_id = [&n, &m](int i, int j) {
        return i * m + j;
    };

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            points.emplace_back((float) i * shift_x, (float) j * shift_y);
            if (i + 1 < n && j + 1 < m) {
                // triangulation
                ids.push_back(get_id(i, j));
                ids.push_back(get_id(i + 1, j));
                ids.push_back(get_id(i, j + 1));

                ids.push_back(get_id(i + 1, j));
                ids.push_back(get_id(i, j + 1));
                ids.push_back(get_id(i + 1, j + 1));
            }
        }
    }

    colors.resize(points.size());
    updPoints();
    updIndexes();
}

void Grid::apply(std::function<Color(Vec2)> &&point_to_color) {
    for (int i = 0; auto &color: colors) {
        color = point_to_color(points[i++]);
    }
    updColors();
}