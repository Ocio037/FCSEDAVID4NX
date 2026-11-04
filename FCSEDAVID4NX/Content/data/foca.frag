#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D baseTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 textureColor = texture(baseTexture, fragUV);
    outColor = fragColor * textureColor;
}
