#version 330 core

uniform vec3 camera_position;
uniform vec3 light_direction;
uniform vec3 bbox_min;
uniform vec3 bbox_max;
uniform sampler3D sampler;
in vec3 position;

uniform sampler2D shadow_map;
uniform mat4 transform;

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

int InShadow(vec3 position) {
    vec4 shadow_pos = transform * vec4(position, 1.0);
    shadow_pos /= shadow_pos.w;
    shadow_pos = shadow_pos * 0.5 + vec4(0.5);

    bool in_shadow_texture = (shadow_pos.x > 0) && (shadow_pos.x < 1) &&
    (shadow_pos.y > 0) && (shadow_pos.y < 1) &&
    (shadow_pos.z > 0) && (shadow_pos.z < 1);

    if (in_shadow_texture) {
        float real_z = texture(shadow_map, shadow_pos.xy).r;
        return int(real_z < shadow_pos.z);
    }

    return 0;
}


void main() {
    if (position.y < 0) discard;
    vec3 from_camera = normalize(position - camera_position);
    vec2 seg = intersect_bbox(camera_position, from_camera);

    float tmin = seg.x;
    float tmax = seg.y;
    tmin = max(tmin, 0.0);

    float absorption = 0.6f;

    const int ITER = 64;
    float dt = (tmax - tmin) / ITER;
    float optical_depth = 0.0;

    for (int i = 0; i < ITER; ++i) {
        float t = tmin + (i + 0.5) * dt;
        vec3 p = camera_position + t * from_camera;
        optical_depth += dt * absorption * (1 - InShadow(p));
    }

    //optical_depth = absorption * (tmax -tmin);
    float opacity = 1.0 - exp(-optical_depth);
    out_color = vec4(1.0, 1.0, 1.0, opacity);
}