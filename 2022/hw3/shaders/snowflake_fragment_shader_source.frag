#version 330 core

layout (location = 0) out vec4 out_color;
in vec2 texcoord;

uniform sampler2D sampler;
uniform sampler1D col;
uniform sampler2D snow;

void main() {
    float dx = texcoord.x - 0.5;
    float dy = texcoord.y - 0.5;

    float alpha = exp(-(dx*dx + dy * dy) * 25);
    if (alpha < 0.5) discard;
    out_color = vec4(texture(snow, texcoord).rgb, alpha);
}