#version 330 core

layout (location = 0) out vec4 out_color;
in vec2 texcoord;
in vec3 position;

uniform sampler2D sampler;
uniform sampler1D col;
uniform sampler2D snow;
uniform sampler2D shadow_map;

uniform mat4x4 transform;

float GetShadowFactor() {
    vec4 shadow_pos = transform * vec4(position, 1.0);
    shadow_pos /= shadow_pos.w;
    shadow_pos = shadow_pos * 0.5 + vec4(0.5);

    bool in_shadow_texture = (shadow_pos.x > 0) && (shadow_pos.x < 1) &&
        (shadow_pos.y > 0) && (shadow_pos.y < 1) &&
        (shadow_pos.z > 0) && (shadow_pos.z < 1);

    if (in_shadow_texture) {
        float real_z = texture(shadow_map, shadow_pos.xy).r;
        return 1.0 - 0.5 * float(real_z < shadow_pos.z);
    }

    return 1.0;
}

float f(float x) {
    return 1;
}

void main() {
    float dx = texcoord.x - 0.5;
    float dy = texcoord.y - 0.5;

    float shadow_factor = GetShadowFactor();
    float alpha = exp(-(dx * dx + dy * dy) * 25);
    if (alpha < 0.5) discard;

    float animation_factor = f(position.y);
    out_color = vec4(texture(snow, texcoord).rgb * shadow_factor, alpha * animation_factor);

    out_color = vec4(vec3(1.f, 1.f, 1.f) * shadow_factor, alpha * animation_factor);
}