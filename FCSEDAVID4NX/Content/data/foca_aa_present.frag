#version 450

layout(location = 0) in vec2 screenUv;
layout(set = 0, binding = 0) uniform sampler2D videoFrameTexture;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PresentSettings
{
    uvec4 values;
} presentSettings;

vec3 EncodedSrgbToLinear(vec3 encoded)
{
    encoded = clamp(encoded, vec3(0.0), vec3(1.0));
    vec3 low = encoded / 12.92;
    vec3 high = pow((encoded + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(encoded, vec3(0.04045)));
}

void main()
{
    // Decode the packed 640x480 YUYV video frame, then present it without changing
    // the existing 720p nearest-neighbor scaling behavior.
    ivec2 pairSize = textureSize(videoFrameTexture, 0);
    ivec2 signalSize = ivec2(pairSize.x * 2, pairSize.y);
    ivec2 signalPixel = clamp(ivec2(screenUv * vec2(signalSize)),
        ivec2(0), signalSize - ivec2(1));
    vec4 packedNormalized = texelFetch(videoFrameTexture,
        ivec2(signalPixel.x / 2, signalPixel.y), 0);
    ivec4 packedYuyv = ivec4(round(packedNormalized * 255.0));
    float y = float((signalPixel.x & 1) == 0 ? packedYuyv.r : packedYuyv.b) - 16.0;
    float u = float(packedYuyv.g) - 128.0;
    float v = float(packedYuyv.a) - 128.0;
    ivec3 decodedCode = ivec3(clamp(vec3(
        1.164 * y + 1.596 * v,
        1.164 * y - 0.392 * u - 0.813 * v,
        1.164 * y + 2.017 * u), vec3(0.0), vec3(255.0)));
    vec3 frameColor = vec3(decodedCode) / 255.0;
    vec3 outputColor = presentSettings.values.w != 0u
        ? EncodedSrgbToLinear(frameColor)
        : frameColor;
    outColor = vec4(outputColor, 1.0);
}
