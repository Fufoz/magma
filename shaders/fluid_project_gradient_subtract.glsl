#version 440

layout(binding = 0) uniform sampler2D velocity_field;
layout(binding = 1) uniform sampler2D pressure_field;

layout(location = 0) out vec4 divergent_free_field;

layout(push_constant) uniform constants
{
   float dx;//grid_scale
}c;

//         _
// u = w - Vp

void main()
{
    float left = texture(pressure_field, vec2(gl_FragCoord.x - 1.0, gl_FragCoord.y)).x;
    float right = texture(pressure_field, vec2(gl_FragCoord.x + 1.0, gl_FragCoord.y)).x;
    float top = texture(pressure_field, vec2(gl_FragCoord.x, gl_FragCoord.y + 1.0)).x;
    float bottom = texture(pressure_field, vec2(gl_FragCoord.x, gl_FragCoord.y - 1.0)).x;

    divergent_free_field = texture(velocity_field, gl_FragCoord.xy);
    divergent_free_field.xy -= 1 / (2 * c.dx) * vec2(right - left, top - bottom);

    //boundary
}