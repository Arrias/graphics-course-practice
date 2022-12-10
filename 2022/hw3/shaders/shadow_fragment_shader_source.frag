#version 330 core
out vec4 out_color;
void main()
{
    float z = gl_FragCoord.z;
    out_color = vec4(z, 0.0, 0.0, 0.0);
    float dF_dx = dFdx(z);
    float dF_dy = dFdy(z);
    float add = 0.25 * (dF_dx * dF_dx + dF_dy * dF_dy);
    out_color = vec4(z, z * z + add, 0.0, 0.0);
}