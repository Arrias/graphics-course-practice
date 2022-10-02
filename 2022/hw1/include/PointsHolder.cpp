#include "PointsHolder.hpp"

PointsHolder::PointsHolder() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &points_vbo);
    glGenBuffers(1, &colors_vbo);
    glGenBuffers(1, &ebo);
    bindArgument<Vec2>(GL_ARRAY_BUFFER, points_vbo, vao, 0, 2, GL_FLOAT, GL_FALSE, (void *) nullptr);
    bindArgument<Color>(GL_ARRAY_BUFFER, colors_vbo, vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, (void *) nullptr);
}

void PointsHolder::updColors() const {
    bindData(GL_ARRAY_BUFFER, colors_vbo, vao, colors);
}

void PointsHolder::updPoints() const {
    bindData(GL_ARRAY_BUFFER, points_vbo, vao, points);
}

void PointsHolder::updIndexes() const {
    bindData(GL_ELEMENT_ARRAY_BUFFER, ebo, vao, ids);
}

size_t PointsHolder::size() const {
    return ids.size();
}