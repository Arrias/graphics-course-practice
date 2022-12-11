#version 330 core
in vec3 position;
in vec3 normal;
in vec2 texcoord;
// удаленный источник
uniform vec3 sun_color;
uniform vec3 camera_position;
uniform vec3 sun_direction;
// точечный источник
uniform float glossiness;
uniform float power;
uniform vec3 point_light_attenuation;
uniform vec3 point_light_color;
uniform vec3 point_light_position;
uniform mat4 transform;
uniform samplerCube depthMap;
uniform float far_plane;
uniform float ambient_light;

uniform int is_wolf;
uniform int use_texture;
uniform vec4 color;
uniform sampler2D albedo;

uniform sampler2D sampler;
uniform sampler2D shadow_map;
layout (location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction, vec3 albedo) {
    return albedo * max(0.0, dot(normal, direction));
}

float GetShadowFactor() {
    float C = 0.005;
    float shadow_factor = 1.0;

    vec4 shadow_pos = transform * vec4(position, 1.0);
    shadow_pos /= shadow_pos.w;
    shadow_pos = shadow_pos * 0.5 + vec4(0.5);
    bool in_shadow_texture = (shadow_pos.x > 0) && (shadow_pos.x < 1) &&
    (shadow_pos.y > 0) && (shadow_pos.y < 1) &&
    (shadow_pos.z > 0) && (shadow_pos.z < 1);

    if (in_shadow_texture) {
        vec2 sum = vec2(0.0, 0.0);
        int r = 3;
        float sum_weight = 0;

        for (int x = -r; x <= r; ++x) {
            for (int y = -r; y <= r; ++y) {
                float expo = exp(-(x * x + y * y) / 8.);
                sum += expo * texture(shadow_map, shadow_pos.xy + vec2(x, y) / textureSize(shadow_map, 0).xy).rg;
                sum_weight += expo;
            }
        }

        vec2 data = sum / sum_weight;
        float mu = data.r;
        float sigma = data.g - mu * mu;
        float z = shadow_pos.z - C;
        float factor = (z < mu) ? 1.0 :
        sigma / (sigma + (z - mu) * (z - mu));

        float l = 0.125;
        if (factor < l) {
            factor = 0.0;;
        } else {
            factor = (factor - l) / (1 - l);
        }

        shadow_factor = factor;
    }
    return shadow_factor;
}

float less_root(float a, float b, float c) {
    // ax^2 + bx + c = 0
    float D = b * b - 4 * a * c;
    return (-b - sqrt(D)) / (2 * a);
}

float intersect_bbox(vec3 origin, vec3 direction)
{
    float A = pow(direction.x, 2) + pow(direction.y, 2) + pow(direction.z, 2);
    float B = 2 * dot(origin, direction);
    float C = pow(origin.x, 2) + pow(origin.y, 2) + pow(origin.z, 2) - 1;
    return less_root(A, B, C);
}

void main() {
    // SUN SHADOW

    float shadow_factor = GetShadowFactor();

    vec3 dir = normalize(position - camera_position);
    float t_max = length(position - camera_position);
    float t_min = intersect_bbox(camera_position, dir);

    if (t_min < 0) t_min = 0.0;
    float absorption = 1;
    float optical_depth = (t_max - t_min) * absorption;
    float opacity = 1.0 - exp(-optical_depth);
    opacity = 1.0;

    if (is_wolf == 0) {
        vec3 albedo = texture(sampler, texcoord).xyz;
        vec3 ambient = albedo * ambient_light;
        vec3 my_color = ambient;
        my_color += diffuse(sun_direction, albedo) * sun_color * shadow_factor;
        out_color = vec4(my_color * opacity, 1.0);
    } else {
        float ambient_bias = 0.2;
        float real_ambient = ambient_light + ambient_bias;
        vec4 albedo_color;
        if (use_texture == 1)
        albedo_color = texture(albedo, texcoord);
        else
        albedo_color = color;
        float diffuse = max(0.0, dot(normalize(normal), sun_direction));
        out_color = vec4(albedo_color.rgb * (real_ambient + diffuse) * shadow_factor * opacity, 1.0);
    }
}