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

vec2 intersect_bbox(vec3 origin, vec3 direction)
{
    vec3 tmin = (bbox_min - origin) / direction;
    vec3 tmax = (bbox_max - origin) / direction;
    sort(tmin.x, tmax.x);
    sort(tmin.y, tmax.y);
    sort(tmin.z, tmax.z);
    return vec2(vmax(tmin), vmin(tmax));
}

vec3 get_texture_pos(vec3 p) {
    return (p - bbox_min) / (bbox_max - bbox_min);
}

const float PI = 3.1415926535;

void main() {
    vec3 from_camera = normalize(position - camera_position);
    vec2 seg = intersect_bbox(camera_position, from_camera);
    float tmin = seg.x;
    float tmax = seg.y;
    tmin = max(tmin, 0.0);

    const int ITER = 64;
    const int JTER = 8;

    vec3 optical_depth = vec3(0.0);
    float absorption = 3.0;
    float dt = (tmax - tmin) / ITER;

    // TASK 5
    vec3 light_color = vec3(16.0);
    vec3 color = vec3(0.0);
    vec3 scattering = vec3(7.f, 1.f, 4.f);
    vec3 extinction = absorption + scattering;
    for (int i = 0; i < ITER; ++i) {
        float t = tmin + (i + 0.5) * dt;
        vec3 p = camera_position + t * from_camera;
        float density = texture(sampler, get_texture_pos(p)).r;

        optical_depth += extinction * density * dt;
        vec3 light_optical_depth = vec3(0.0);
        vec2 light_seg = intersect_bbox(p, light_direction);

        float tmin_l = max(light_seg.x, 0.0);
        float tmin_r = light_seg.y;
        float dx_light = (tmin_r - tmin_l) / JTER;
        for (int j = 0; j < JTER; ++j) {
            float l = tmin_l + (j + 0.5) * dx_light;
            vec3 l_p = p + l * light_direction;
            light_optical_depth += extinction * texture(sampler, get_texture_pos(l_p)).r * dx_light;
        }

        color += light_color * exp(- light_optical_depth) * exp(- optical_depth) * dt * texture(sampler, get_texture_pos(p)).r * scattering / 4.0 / PI;
    }

    vec3 opacity = 1.0 - exp(-optical_depth);
    out_color = vec4(color * opacity, 1.0);
}