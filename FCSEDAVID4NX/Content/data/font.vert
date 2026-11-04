#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inUvLayer;

layout(location = 0) out vec3 fragUvLayer;

layout(set = 0, binding = 1, std140) uniform TransformData
{
    mat4 Mtx;
    mat4 Projection;
    mat4 World;
} transform;

void main()
{
    gl_Position = transform.Projection * transform.World * transform.Mtx * vec4(inPosition, 1.0);
    fragUvLayer = inUvLayer;
}
