#include "Graph.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>

using std::make_pair;
const std::vector<std::pair<int, int>> po[16] = {
        {},
        {make_pair(2, 3)},
        {make_pair(1, 2)},
        {make_pair(1, 3)},
        {make_pair(0, 1)},
        {make_pair(0, 3), make_pair(1, 2)},
        {make_pair(0, 2)},
        {make_pair(0, 3)},
        {make_pair(0, 3)},
        {make_pair(0, 2)},
        {make_pair(0, 1), make_pair(2, 3)},
        {make_pair(0, 1)},
        {make_pair(1, 3)},
        {make_pair(1, 2)},
        {make_pair(2, 3)},
        {}
};

void Graph::buildGrid(int nn, int mm, int width, int height) {
    n = nn;
    m = mm;

    grid.points.clear();
    grid.ids.clear();

//    auto get_id = [&n, &m](int i, int j) {
//        return i * m + j;
//    };

    float shift_x = (float) width / (float) (n - 1);
    float shift_y = (float) height / (float) (m - 1);

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            grid.points.emplace_back((float) i * shift_x, (float) j * shift_y);
            if (i + 1 < n && j + 1 < m) {
                // triangulation
                grid.ids.push_back(get_id(i, j));
                grid.ids.push_back(get_id(i + 1, j));
                grid.ids.push_back(get_id(i, j + 1));

                grid.ids.push_back(get_id(i + 1, j));
                grid.ids.push_back(get_id(i, j + 1));
                grid.ids.push_back(get_id(i + 1, j + 1));
            }
        }
    }

    //  grid.colors.resize(grid.points.size());
    grid.updPoints();
    grid.updIndexes();
}

int Graph::get_id(int i, int j) const {
    return i * m + j;
}

template<class T>
void log(T x) {
    std::cout << x << std::endl;
}

void log(Vec2 x) {
    std::cout << x.x << " " << x.y << std::endl;
}

void Graph::apply(std::function<float(Vec2, Color &)> &&point_to_color) {
    dst.assign(n, std::vector<float>(m));
    grid.colors.resize(n * m);

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            dst[i][j] = point_to_color(grid.points[get_id(i, j)], grid.colors[get_id(i, j)]);
        }
    }

    grid.updColors();
    lines.points.clear();
    lines.ids.clear();
}

void Graph::draw() const {
    glLineWidth(10);
    glBindVertexArray(grid.vao);
    glDrawElements(GL_TRIANGLES, (GLsizei) grid.size(), GL_UNSIGNED_INT, (void *) nullptr);
    glBindVertexArray(lines.vao);
    glDrawElements(GL_LINES, (GLsizei) lines.size(), GL_UNSIGNED_INT, (void *) nullptr);
}

Graph::Graph(int nn, int mm, int width, int height) {
    buildGrid(nn, mm, width, height);
}

void Graph::addLine(float C) {
    h_points.assign(n, std::vector<int>(m, -1));
    v_points.assign(n, std::vector<int>(m, -1));

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            if (i + 1 < n && lesser(dst[i][j], C) != lesser(dst[i + 1][j], C)) {
                int me = get_id(i, j);
                int nxt = get_id(i + 1, j);
                float x = (C - dst[i][j]) / (dst[i + 1][j] - dst[i][j]);

                auto beg = grid.points[me];
                auto end = grid.points[nxt];

                h_points[i][j] = lines.points.size();
                lines.points.emplace_back(beg + x * (end - beg));
            }
            if (j + 1 < m && lesser(dst[i][j], C) != lesser(dst[i][j + 1], C)) {
                int me = get_id(i, j);
                int nxt = get_id(i, j + 1);
                float x = (C - dst[i][j]) / (dst[i][j + 1] - dst[i][j]);

                auto beg = grid.points[me];
                auto end = grid.points[nxt];

                v_points[i][j] = lines.points.size();
                lines.points.emplace_back(beg + x * (end - beg));
            }
        }
    }

    std::vector<int> nums;
    for (int i = 0; i + 1 < n; ++i) {
        for (int j = 0; j + 1 < m; ++j) {
            nums.clear();
            nums.push_back(v_points[i][j]);
            nums.push_back(h_points[i][j + 1]);
            nums.push_back(v_points[i + 1][j]);
            nums.push_back(h_points[i][j]);

            int mask = 0;
            mask |= 1 * !lesser(dst[i][j], C);
            mask |= 2 * !lesser(dst[i][j + 1], C);
            mask |= 4 * !lesser(dst[i + 1][j + 1], C);
            mask |= 8 * !lesser(dst[i + 1][j], C);

            for (auto [x, y]: po[mask]) {
                if (nums[x] == -1 || nums[y] == -1) continue;
                lines.ids.push_back(nums[x]);
                lines.ids.push_back(nums[y]);
            }
        }
    }

    lines.colors.assign(lines.points.size(), YellowColor());
    lines.updIndexes();
    lines.updColors();
    lines.updPoints();
}
