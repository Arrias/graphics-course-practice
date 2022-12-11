#version 330 core

uniform vec3 camera_position;
uniform vec3 light_direction;
uniform vec3 bbox_min;
uniform vec3 bbox_max;
uniform sampler3D sampler;
in vec3 position;

layout (location = 0) out vec4 out_color;

void sort(inout float x, inout float y)
{
    if (x > y)
    {
        float t = x;
        x = y;
        y = t;
    }
}

float vmin(vec3 v)
{
    return min(v.x, min(v.y, v.z));
}

float vmax(vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

vec2 solve(float a, float b, float c) {
    // ax^2 + bx + c = 0
    float D = b * b - 4 * a * c;
    float x1 = (-b - sqrt(D)) / (2 * a);
    float x2 = (-b + sqrt(D)) / (2 * a);
    return vec2(x1, x2);
}

vec2 intersect_bbox(vec3 origin, vec3 direction)
{
    float A = pow(direction.x, 2) + pow(direction.y, 2) + pow(direction.z, 2);
    float B = 2 * dot(origin, direction);
    float C = pow(origin.x, 2) + pow(origin.y, 2) + pow(origin.z, 2) - 1;
    return solve(A, B, C);
}

vec3 get_texture_pos(vec3 p) {
    return (p - bbox_min) / (bbox_max - bbox_min);
}

const float PI = 3.1415926535;

void main() {
    if (position.y < 0) discard;
    vec3 from_camera = normalize(position - camera_position);
    vec2 seg = intersect_bbox(camera_position, from_camera);

    float tmin = seg.x;
    float tmax = seg.y;
    tmin = max(tmin, 0.0);

    float absorption = 0.5f;
    float optical_depth = (tmax - tmin) * absorption;

    float opacity = 1.0 - exp(-optical_depth);
    out_color = vec4(1.0, 1.0, 1.0, opacity);
}