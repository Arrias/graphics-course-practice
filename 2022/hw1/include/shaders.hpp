#pragma once
#include <string>

const std::string vertex_shader_source =
        R"(#version 330 core
           uniform mat4 view;

           layout (location = 0) in vec2 in_position;
           layout (location = 1) in vec4 in_color;

           out vec4 color;
           void main()
           {
                gl_Position = view * vec4(in_position, 0.0, 1.0);
                color = in_color;
           }
)";

const std::string fragment_shader_source =
        R"(#version 330 core
            in vec4 color;
            layout (location = 0) out vec4 out_color;
            void main()
            {
                out_color = color;
            }
)";