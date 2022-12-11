#version 330 core
uniform mat4 model;
uniform mat4 transform;
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in ivec4 in_joints;
layout (location = 4) in vec4 in_weights;

uniform mat4x3 bones[64];
uniform int is_wolf;

mat4x3 GetMean() {
    return in_weights.x * bones[in_joints.x] +
    in_weights.y * bones[in_joints.y] +
    in_weights.z * bones[in_joints.z] +
    in_weights.w * bones[in_joints.w];
}

void main() {
    mat4x3 average = GetMean();
    if (is_wolf == 0) {
        gl_Position = transform * model * vec4(in_position, 1.0);
    } else {
        gl_Position = transform * model * mat4(average) * vec4(in_position, 1.0);
    }
}