#version 450

layout(location = 0) in vec3 fragUvLayer;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2DArray fontAtlas;

layout(push_constant) uniform FontColor
{
    vec4 color;
} textColor;

void main()
{
    vec4 atlasColor = texture(fontAtlas, fragUvLayer);
    outColor = vec4(atlasColor.rgb * textColor.color.rgb, atlasColor.a * textColor.color.a);
}
