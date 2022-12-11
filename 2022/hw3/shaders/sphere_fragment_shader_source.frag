#version 330 core

uniform vec3 light_direction;
uniform vec3 camera_position;

uniform sampler2D albedo_texture;
uniform sampler2D normal_texture;
uniform sampler2D environment_texture;
uniform float sphere_y_mid;

in vec3 position;
in vec3 tangent;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;

float CalcOpacity(vec3 normal, vec3 view_direction) {
    const float n = 2; // коэффициент преломления
    float cos_theta = abs(dot(normal, view_direction));
    float R0 = pow((1 - n) / (1 + n), 2.0);
    float R = R0 + (1 - R0) * pow(1 - cos_theta, 5.0);
    return R;
}

void main()
{
    float ambient_light = 0.2;
    vec3 bitangent = cross(tangent, normal);
    mat3 tbn = mat3(tangent, bitangent, normal);
    vec3 real_normal = tbn * (texture(normal_texture, texcoord).rgb * 2.0 - vec3(1.0));

    // нормаль
    real_normal = normalize(mix(normal, real_normal, 0.5));
    real_normal = normal;

    // направление на камеру
    vec3 to_camera = normalize(position - camera_position);

    // вычисляем, куда луч отражается
    vec3 dir = normalize(reflect(to_camera, real_normal));

    float x = atan(dir.z, dir.x) / PI * 0.5 + 0.5;
    float y = - atan(dir.y, length(dir.xz)) / PI + 0.5;

    vec3 env_albedo = texture(environment_texture, vec2(x, y)).rgb;

    float lightness = ambient_light + max(0.0, dot(normalize(real_normal), light_direction));

    vec3 albedo = texture(albedo_texture, texcoord).rgb;
    vec3 color = (lightness * albedo + env_albedo) / 2; // Practice10
    color = lightness * env_albedo;

    float opacity = CalcOpacity(real_normal, to_camera);

    out_color = vec4(color, opacity);

    if (position.y < sphere_y_mid) { // SPHERE CONDITION
        albedo *= lightness;
        out_color = vec4(albedo, 1.0);
    }
}