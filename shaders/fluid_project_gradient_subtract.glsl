#version 440

#include "fluid_boundary.h.glsl"

layout(binding = 0) uniform sampler2D velocity_field;
layout(binding = 1) uniform sampler2D pressure_field;

layout(location = 0) out vec4 divergent_free_field;
layout(location = 1) in vec2 samplePos;

layout(push_constant) uniform constants
{
   float dx;//grid_scale
}c;

//         _
// u = w - Vp

void main()
{
    float texelSize = c.dx;
    float left = sample_pressure_field(pressure_field, vec2(samplePos.x - texelSize, samplePos.y), texelSize).x;
    float right = sample_pressure_field(pressure_field, vec2(samplePos.x + texelSize, samplePos.y), texelSize).x;
    float top = sample_pressure_field(pressure_field, vec2(samplePos.x, samplePos.y + texelSize), texelSize).x;
    float bottom = sample_pressure_field(pressure_field, vec2(samplePos.x, samplePos.y - texelSize), texelSize).x;
    // float left = texture(pressure_field, vec2(samplePos.x - texelSize, samplePos.y)).x;
    // float right = texture(pressure_field, vec2(samplePos.x + texelSize, samplePos.y)).x;
    // float top = texture(pressure_field, vec2(samplePos.x, samplePos.y + texelSize)).x;
    // float bottom = texture(pressure_field, vec2(samplePos.x, samplePos.y - texelSize)).x;

    divergent_free_field = sample_velocity_field(velocity_field, samplePos, texelSize);
    divergent_free_field.xy -= 1 / (2 * c.dx) * vec2(right - left, top - bottom);

}