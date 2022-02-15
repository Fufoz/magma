#version 440

layout(binding = 0) uniform sampler2D inkTexture;
layout(location = 0) out vec4 outputColor;
layout(location = 1) in vec2 samplePos;

void main()
{
    outputColor = texture(inkTexture, samplePos);
}