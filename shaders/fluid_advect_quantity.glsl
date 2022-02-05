#version 450

layout (binding = 0) uniform sampler2D velocity_sampler;
layout (binding = 1) uniform sampler2D quality_to_advect;

layout (location = 0) out vec4 render_target_output;

layout(push_constant) uniform constants
{
	float grid_scale;
    float time_step;
}sim_constants;

vec4 bilinear_filter(sampler2D target, vec2 position)
{
    float x00 = floor(position.x);
    float y00 = floor(position.y);

    vec2 fraction = position - vec2(x00, y00);
    
    vec4 val00 = texture(target, vec2(x00, y00));
    vec4 val01 = texture(target, vec2(x00 + 1.0, y00));
    vec4 val10 = texture(target, vec2(x00, y00 + 1.0));
    vec4 val11 = texture(target, vec2(x00 + 1.0, y00 + 1.0));

    return mix(mix(val00, val01, fraction.x), mix(val10, val11, fraction.x), fraction.y);
}

void main()
{
    vec2 sample_from_position = gl_FragCoord.xy - 
        texture(velocity_sampler, gl_FragCoord.xy).xy * 
        sim_constants.time_step * sim_constants.grid_scale;
    
    render_target_output = bilinear_filter(quality_to_advect, sample_from_position);
}

