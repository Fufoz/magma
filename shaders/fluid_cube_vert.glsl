#version 440

layout(location = 0) in vec3 vertexCoord;

void main()
{
    gl_Position = vec4(vertexCoord, 1.0);
}