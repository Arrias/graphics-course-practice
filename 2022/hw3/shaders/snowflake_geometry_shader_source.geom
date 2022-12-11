#version 330 core
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 camera_position;

layout (points) in ;
layout (triangle_strip, max_vertices = 4) out ;
out vec2 texcoord;
out vec3 position;
in float in_angle[];

vec3 perp(vec3 v) {
    if (v.x == 0 && v.y == 0 && v.z == 1) {
        return cross(v, vec3(1.0, 0.0, 0.0));
    }
    return cross(v, vec3(0.0, 0.0, 1.0));
}

void main()
{
    float angle = in_angle[0];
    vec3 center = gl_in[0].gl_Position.xyz;
    float size = gl_in[0].gl_PointSize;
    const float signs[2] = float[2](-1.0, 1.0);
    const float dx[4] = float[4](-0.5, 0.5, -0.5, 0.5);
    const float dy[4] = float[4](-0.5, -0.5, 0.5, 0.5);
    vec3 Z = normalize(camera_position - center);
    vec3 X = normalize(cross(Z, vec3(0.0, 1.0, 0.0)));
    vec3 Y = normalize(cross(X, Z));
    vec3 X_s = X * cos(angle) + Y * sin(angle);
    vec3 Y_s = -X * sin(angle) + Y * cos(angle);
    X = X_s;
    Y = Y_s;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            vec3 add = X * size * signs[i] + Y * size * signs[j];
            gl_Position = projection * view * model * vec4(center + add, 1.0);
            position = (model * vec4(center+add, 1.0)).xyz;
            int n = 2 * i + j;
            texcoord = vec2(0.5, 0.5) + vec2(dx[n], dy[n]);
            EmitVertex();
        }
    }
    EndPrimitive();
}