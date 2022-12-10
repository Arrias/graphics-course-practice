#version 330 core
uniform sampler2D sampler;
in vec2 texcoord;
layout (location = 0) out vec4 out_color;
void main()
{
    out_color = vec4(texture(sampler, texcoord).rgb, 1.0);
}