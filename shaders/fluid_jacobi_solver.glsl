#version 440

#include "fluid_boundary.h.glsl"

//Ax=b

layout (binding = 0) uniform sampler2D x;
layout (binding = 1) uniform sampler2D b;

layout (location = 0) out vec4 jacobi_output;
layout (location = 1) in vec2 samplePos;

layout(push_constant) uniform const_block
{
    float alpha;
    float beta;
    float texelSize;
}jacobi_constants;

void main()
{
    // vec4 x_left =  texture(x, vec2(samplePos.x - jacobi_constants.texelSize, samplePos.y));
    // vec4 x_right =  texture(x, vec2(samplePos.x + jacobi_constants.texelSize, samplePos.y));
    // vec4 x_top =  texture(x, vec2(samplePos.x, samplePos.y + jacobi_constants.texelSize));
    // vec4 x_bottom =  texture(x, vec2(samplePos.x, samplePos.y - jacobi_constants.texelSize));
    // vec4 b_center = texture(b, samplePos);
    float tsize = jacobi_constants.texelSize;
#ifdef PRESSURE_SOLVER
    vec4 x_left = sample_pressure_field(x, vec2(samplePos.x - tsize, samplePos.y), tsize);
    vec4 x_right = sample_pressure_field(x, vec2(samplePos.x + tsize, samplePos.y), tsize);
    vec4 x_top = sample_pressure_field(x, vec2(samplePos.x, samplePos.y + tsize), tsize);
    vec4 x_bottom = sample_pressure_field(x, vec2(samplePos.x, samplePos.y - tsize), tsize);
    vec4 b_center = texture(b, samplePos);
#else 
    vec4 x_left = sample_velocity_field(x, vec2(samplePos.x - tsize, samplePos.y), tsize);
    vec4 x_right = sample_velocity_field(x, vec2(samplePos.x + tsize, samplePos.y), tsize);
    vec4 x_top = sample_velocity_field(x, vec2(samplePos.x, samplePos.y + tsize), tsize);
    vec4 x_bottom = sample_velocity_field(x, vec2(samplePos.x, samplePos.y - tsize), tsize);
    vec4 b_center = sample_velocity_field(b, samplePos, tsize);
#endif

    jacobi_output = (x_left + x_right + x_top + x_bottom + jacobi_constants.alpha * b_center) / jacobi_constants.beta;
    
}