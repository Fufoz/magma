#version 440

layout(location = 0) in vec3 vertexCoord;
layout(location = 1) out vec2 samplePos; 

void main()
{
    gl_Position = vec4(vertexCoord, 1.0);
    samplePos = gl_Position.xy * 0.5 + 0.5;
}