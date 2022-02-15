#version 440
#include "fluid_boundary.h.glsl"

layout(binding = 0) uniform sampler2D velocity_field;

layout(location = 0) out vec4 divergence;
layout(location = 1) in vec2 samplePos;

layout(push_constant) uniform constants
{
   float dx;//grid_scale
}c;

void main()
{
    float texelSize = c.dx;

    float left = sample_velocity_field(velocity_field, vec2(samplePos.x - texelSize, samplePos.y), texelSize).x; 
    float right = sample_velocity_field(velocity_field, vec2(samplePos.x + texelSize, samplePos.y), texelSize).x; 
    float top = sample_velocity_field(velocity_field, vec2(samplePos.x, samplePos.y + texelSize), texelSize).y; 
    float bottom = sample_velocity_field(velocity_field, vec2(samplePos.x, samplePos.y - texelSize), texelSize).y; 

    divergence = vec4(((right - left) + (top - bottom)) / (2 * c.dx), 0.0, 0.0, 1.0);
}