#version 450

layout (binding = 0) uniform sampler2D velocity_sampler;
layout (binding = 1) uniform sampler2D quality_to_advect;

layout (location = 0) out vec4 render_target_output;
layout (location = 1) in vec2 samplePos;

layout(push_constant) uniform constants
{
	float grid_scale;
    float time_step;
    float dissipation;
}sim_constants;

vec4 bilinear_filter(sampler2D target, vec2 position)
{
    float tsize = sim_constants.grid_scale;

    vec2 screenSpacePos = position / tsize - 0.5;
    vec2 topLeft = floor(screenSpacePos);
    vec2 fraction = screenSpacePos - topLeft;

    vec2 topLeftNormalised = tsize * (topLeft + 0.5);

    vec4 val00 = texture(target, vec2(topLeftNormalised));
    vec4 val01 = texture(target, vec2(topLeftNormalised.x + tsize, topLeftNormalised.y));
    vec4 val10 = texture(target, vec2(topLeftNormalised.x, topLeftNormalised.y + tsize));
    vec4 val11 = texture(target, vec2(topLeftNormalised.x + tsize, topLeftNormalised.y + tsize));

    return mix(mix(val00, val01, fraction.x), mix(val10, val11, fraction.x), fraction.y);
}

void main()
{
    float dt = sim_constants.time_step;
    float dx = sim_constants.grid_scale;

    // vec2 sample_from_position = samplePos - 
    //     texture(velocity_sampler, samplePos).xy * 
    //     sim_constants.time_step * sim_constants.grid_scale;
    
    //runge-kutta 3rd order
    vec2 k1 = texture(velocity_sampler, samplePos).xy;
    vec2 k2 = texture(velocity_sampler, samplePos - 0.5 * k1 * dt * dx).xy;
    vec2 k3 = texture(velocity_sampler, samplePos - 0.75 * k2 * dt * dx).xy;
    vec2 sample_from_position = samplePos - dt * dx * (0.2222 * k1 + 0.3333 * k2 + 0.4444 * k3);

    // render_target_output = bilinear_filter(quality_to_advect, sample_from_position);
    render_target_output = sim_constants.dissipation * texture(quality_to_advect, sample_from_position);
}

