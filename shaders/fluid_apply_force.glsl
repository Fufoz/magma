#version 440

layout(push_constant) uniform force_data
{
    vec4 force_color;
    vec2 mouse_pos;
    float impulse_radius;
}data;

layout(binding = 0) uniform sampler2D input_velocity;
layout(location = 0) out vec4 outputVelocity;
layout(location = 1) in vec2 samplePos;

void main()
{
    // vec2 distance = data.mouse_pos - gl_FragCoord.xy;
    vec2 distance = data.mouse_pos - samplePos;
    vec4 splat = data.force_color * exp(-(dot(distance, distance) / 
        (2.0 * data.impulse_radius * data.impulse_radius)));
    outputVelocity = texture(input_velocity, samplePos) + splat;
}