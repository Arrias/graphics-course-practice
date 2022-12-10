#version 330 core

vec2 vertices[6] = vec2[6](
vec2(-1.0, -1.0),
vec2(1.0, -1.0),
vec2(1.0, 1.0),
vec2(-1.0, -1.0),
vec2(1.0, 1.0),
vec2(-1.0, 1.0)
);

uniform mat4 view_projection_inverse;

out vec3 position;
void main() {
    vec2 vertex = vertices[gl_VertexID];
    vec4 ndc = vec4(vertex, 0.0, 1.0);
    vec4 clip_space = view_projection_inverse * ndc;
    position = clip_space.xyz / clip_space.w;
    gl_Position = vec4(vertex, 0.0, 1.0);
}