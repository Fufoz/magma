#version 440

layout(binding = 0) uniform sampler2D vorticity_field;
layout(binding = 1) uniform sampler2D velocity_field;

layout(location = 0) out vec4 output_velocity;
layout(location = 1) in vec2 samplePos;

layout(push_constant) uniform constants
{
    float confinement;
    float timestep;
    float texelSize;
}vortConsts;

void main()
{
    float dx = vortConsts.texelSize;
    float dt = vortConsts.timestep;
    float conf = vortConsts.confinement;

    //first compute gradient of vorticity field
    float vort_left = texture(vorticity_field, vec2(samplePos.x - dx, samplePos.y)).x;
    float vort_right = texture(vorticity_field, vec2(samplePos.x + dx, samplePos.y)).x;
    float vort_top = texture(vorticity_field, vec2(samplePos.x, samplePos.y + dx)).x;
    float vort_bottom = texture(vorticity_field, vec2(samplePos.x, samplePos.y - dx)).x;
    float vort_center = texture(vorticity_field, samplePos).x;

    vec2 force = vec2(abs(vort_top) - abs(vort_bottom), abs(vort_right) - abs(vort_left)) / (2 * dx);

    float epsilon = 2.4414e-4;
    float divTerm = max(epsilon, dot(force, force));
    force = force * inversesqrt(divTerm);

    force *= conf * vort_center * vec2(1, -1);

    output_velocity = texture(velocity_field, samplePos) + vec4(dt * force, 0.0, 1.0);
}