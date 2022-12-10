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

float ShadowCalculation() {
    vec3 fragToLight = position - point_light_position;
    //return texture(depthMap, fragToLight).r;;
    float closestDepth = texture(depthMap, fragToLight).r;
    closestDepth *= far_plane;
    float currentDepth = length(fragToLight);
    //        //if (currentDepth < 100.f) {
    //          //  return 0.0;
    //        //}
    float bias = 1.5;
    float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
    return shadow;
}

uniform sampler2D sampler;
uniform sampler2D shadow_map;
layout (location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction, vec3 albedo) {
    return albedo * max(0.0, dot(normal, direction));
}

float C = 0.005;

void main() {
    // SUN SHADOW
    vec4 shadow_pos = transform * vec4(position, 1.0);
    shadow_pos /= shadow_pos.w;
    shadow_pos = shadow_pos * 0.5 + vec4(0.5);
    bool in_shadow_texture = (shadow_pos.x > 0) && (shadow_pos.x < 1) &&
    (shadow_pos.y > 0) && (shadow_pos.y < 1) &&
    (shadow_pos.z > 0) && (shadow_pos.z < 1);

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

    float shadow_factor = 1.0;
    if (in_shadow_texture)
    shadow_factor = factor;

    vec3 albedo = texture(sampler, texcoord).xyz;
vec3 ambient = albedo * ambient_light;
vec3 color = ambient;
color += diffuse(sun_direction, albedo) * sun_color * shadow_factor;
out_color = vec4(color, 1.0);
}