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
    vec4 left = sample_velocity_field(velocity_field, vec2(samplePos.x - texelSize, samplePos.y), texelSize); 
    vec4 right = sample_velocity_field(velocity_field, vec2(samplePos.x + texelSize, samplePos.y), texelSize); 
    vec4 top = sample_velocity_field(velocity_field, vec2(samplePos.x, samplePos.y + texelSize), texelSize); 
    vec4 bottom = sample_velocity_field(velocity_field, vec2(samplePos.x, samplePos.y - texelSize), texelSize); 
    // vec4 left = texture(velocity_field, vec2(samplePos.x - texelSize, samplePos.y));
    // vec4 right = texture(velocity_field, vec2(samplePos.x + texelSize, samplePos.y));
    // vec4 top = texture(velocity_field, vec2(samplePos.x, samplePos.y + texelSize));
    // vec4 bottom = texture(velocity_field, vec2(samplePos.x, samplePos.y - texelSize));

    divergence = ((right - left) + (top - bottom)) / (2 * c.dx);
}