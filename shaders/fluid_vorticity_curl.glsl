#version 440

layout(binding = 0) uniform sampler2D velocity_field;
layout(location = 0) out vec4 vorticity_field;
layout(location = 1) in vec2 samplePos;

layout(push_constant) uniform constants
{
    float texelSize;
}vortConsts;

void main()
{
    float dx = vortConsts.texelSize;
    float left = texture(velocity_field, vec2(samplePos.x - dx, samplePos.y)).y;
    float right = texture(velocity_field, vec2(samplePos.x + dx, samplePos.y)).y;
    float top = texture(velocity_field, vec2(samplePos.x, samplePos.y + dx)).x;
    float bottom = texture(velocity_field, vec2(samplePos.x, samplePos.y - dx)).x;

    vorticity_field  = vec4(((right - left) - (top - bottom)) / (2 * dx), 0.0, 0.0, 1.0);
}