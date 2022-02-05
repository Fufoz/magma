#version 440

//Ax=b

layout (binding = 0) uniform sampler2D x;
layout (binding = 1) uniform sampler2D b;
layout (location = 0) out vec4 jacobi_output;

layout(push_constant) uniform const_block
{
    float alpha;
    float beta;
}jacobi_constants;

void main()
{
    vec4 x_left = texture(x, gl_FragCoord.xy - (1.0, 0.0));
    vec4 x_right = texture(x, gl_FragCoord.xy + (1.0, 0.0));
    vec4 x_top = texture(x, gl_FragCoord.xy + (0.0, 1.0));
    vec4 x_bottom = texture(x, gl_FragCoord.xy - (0.0, 1.0));

    vec4 b_center = texture(b, gl_FragCoord.xy);

    jacobi_output = (x_left + x_right + x_top + x_bottom + jacobi_constants.alpha * b_center) / jacobi_constants.beta;
    
}