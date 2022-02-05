#version 440

layout(binding = 0) uniform sampler2D velocity_field;

layout(location = 0) out vec4 divergence;

layout(push_constant) uniform constants
{
   float dx;//grid_scale
}c;

void main()
{
    vec4 left = texture(velocity_field, vec2(gl_FragCoord.x - 1.0, gl_FragCoord.y));
    vec4 right = texture(velocity_field, vec2(gl_FragCoord.x + 1.0, gl_FragCoord.y));
    vec4 top = texture(velocity_field, vec2(gl_FragCoord.x, gl_FragCoord.y + 1.0));
    vec4 bottom = texture(velocity_field, vec2(gl_FragCoord.x, gl_FragCoord.y - 1.0));

    divergence = ((right - left) + (top - bottom)) / (2 * c.dx);
}