#version 330 core
in vec3 position;
uniform sampler2D env;
uniform vec3 camera_position;
uniform float lightness;

layout (location = 0) out vec4 out_color;
const float PI = 3.141592653589793;

void main() {
    vec3 dir = normalize(position - camera_position);
    float x = atan(dir.z, dir.x) / PI * 0.5 + 0.5;
    float y = - atan(dir.y, length(dir.xz)) / PI + 0.5;
    vec3 albedo = texture(env, vec2(x, y)).rgb;
    out_color = vec4(albedo * lightness, 1.0);
}