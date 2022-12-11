#version 330 core

uniform mat4 view;
uniform mat4 projection;
uniform vec3 bbox_min;
uniform vec3 bbox_max;
uniform mat4 model;

layout (location = 0) in vec3 in_position;
out vec3 position;

void main() {
    position = (model * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
}