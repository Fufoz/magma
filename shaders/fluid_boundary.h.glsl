
vec4 sample_pressure_field(sampler2D pressureTexture, vec2 position, float texelSize)
{
    vec2 offsets = vec2(0.0, 0.0);

    if(position.x < 0.0)
    {
        offsets.x = texelSize;
    }
    else if(position.x > 1.0)
    {
        offsets.x = -texelSize;
    }

    if(position.y < 0.0)
    {
        offsets.y = texelSize;
    }
    else if(position.y > 1.0)
    {
        offsets.y = -texelSize;
    }

    return texture(pressureTexture, position + offsets);
}

vec4 sample_velocity_field(sampler2D velocityTexture, vec2 position, float texelSize)
{
    vec2 offsets = vec2(0.0, 0.0);
    float scale = 1.0;

    if(position.x < 0.0)
    {
        offsets.x = texelSize;
        scale = -1.0;
    }
    else if(position.x > 1.0)
    {
        offsets.x = -texelSize;
        scale = -1.0;
    }

    if(position.y < 0.0)
    {
        offsets.y = texelSize;
        scale = -1.0;
    }
    else if(position.y > 1.0)
    {
        offsets.y = -texelSize;
        scale = -1.0;
    }

    return scale * texture(velocityTexture, position + offsets);
}
