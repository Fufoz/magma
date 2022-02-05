#version 440

layout(binding = 0) uniform sampler2D inkTexture;
layout(location = 0) out vec4 outputColor;

void main()
{
    outputColor = texture(inkTexture, gl_FragCoord.xy);
}