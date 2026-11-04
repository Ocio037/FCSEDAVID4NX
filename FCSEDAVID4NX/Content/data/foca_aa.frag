#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;

layout(location = 0) out vec4 outColor;

const ivec2 VideoSignalSize = ivec2(640, 480);
const ivec2 VideoPairSize = ivec2(320, 480);

layout(push_constant) uniform CopyFilterData
{
    uvec4 rowWeights;
} copyFilter;

ivec2 ClampScenePixel(ivec2 pixel, ivec2 resolution)
{
    return clamp(pixel, ivec2(0), resolution - ivec2(1));
}

vec3 SampleSceneBilinear(vec2 pixelPosition)
{
    ivec2 sceneSize = textureSize(sceneTexture, 0);
    vec2 texelPosition = pixelPosition - vec2(0.5);
    ivec2 firstPixel = ivec2(floor(texelPosition));
    vec2 blend = fract(texelPosition);
    ivec2 topLeft = ClampScenePixel(firstPixel, sceneSize);
    ivec2 topRight = ClampScenePixel(firstPixel + ivec2(1, 0), sceneSize);
    ivec2 bottomLeft = ClampScenePixel(firstPixel + ivec2(0, 1), sceneSize);
    ivec2 bottomRight = ClampScenePixel(firstPixel + ivec2(1, 1), sceneSize);
    vec3 top = mix(texelFetch(sceneTexture, topLeft, 0).rgb,
        texelFetch(sceneTexture, topRight, 0).rgb, blend.x);
    vec3 bottom = mix(texelFetch(sceneTexture, bottomLeft, 0).rgb,
        texelFetch(sceneTexture, bottomRight, 0).rgb, blend.x);
    return mix(top, bottom, blend.y);
}

uvec3 QuantizeRgb565(vec3 color)
{
    uvec3 quantized = uvec3(roundEven(clamp(color, vec3(0.0), vec3(1.0)) * 255.0));
    quantized.r = (quantized.r & 0xf8u) | (quantized.r >> 5u);
    quantized.g = (quantized.g & 0xfcu) | (quantized.g >> 6u);
    quantized.b = (quantized.b & 0xf8u) | (quantized.b >> 5u);
    return quantized;
}

vec2 GetWiiSamplePosition(int sampleIndex, bool patternB)
{
    if (!patternB)
    {
        if (sampleIndex == 0) return vec2(3.0, 2.0);
        if (sampleIndex == 1) return vec2(9.0, 6.0);
        return vec2(3.0, 10.0);
    }
    if (sampleIndex == 0) return vec2(9.0, 2.0);
    if (sampleIndex == 1) return vec2(3.0, 6.0);
    return vec2(9.0, 10.0);
}

uvec3 SampleSceneColor(ivec2 pixel, float sourceY)
{
    ivec2 sceneSize = textureSize(sceneTexture, 0);
    bool patternB = (pixel.x & 2) != 0;
    uvec3 colorSum = uvec3(0u);
    for (int sampleIndex = 0; sampleIndex < 3; ++sampleIndex)
    {
        // Approximate the Wii's three subpixel locations from the single-sample
        // scene image; this smooths coverage edges without requesting MSAA.
        vec2 samplePosition = GetWiiSamplePosition(sampleIndex, patternB) / 12.0;
        vec2 scenePosition = vec2(float(pixel.x), sourceY) + samplePosition;
        scenePosition = clamp(scenePosition, vec2(0.0), vec2(sceneSize));
        colorSum += QuantizeRgb565(SampleSceneBilinear(scenePosition));
    }
    return colorSum / 3u;
}

uvec3 CopySceneColorToVideoFrame(ivec2 pixel)
{
    ivec2 sceneSize = textureSize(sceneTexture, 0);
    pixel = clamp(pixel, ivec2(0), VideoSignalSize - ivec2(1));

    // Map output pixel centers to the source, then sample the adjacent source
    // rows from source pixel centers.
    float sourceY = (float(pixel.y) + 0.5) * float(sceneSize.y) /
        float(VideoSignalSize.y) - 0.5;
    uvec3 previous = SampleSceneColor(pixel, sourceY - 1.0);
    uvec3 current = SampleSceneColor(pixel, sourceY);
    uvec3 next = SampleSceneColor(pixel, sourceY + 1.0);

    // Apply the native NTSC 480p AA vertical copy filter; the seven taps are
    // grouped into three source rows, then divided by 64.
    uvec3 filtered = previous * copyFilter.rowWeights.x +
        current * copyFilter.rowWeights.y + next * copyFilter.rowWeights.z;
    return min(filtered >> 6u, uvec3(255u));
}

ivec3 ConvertRgbToYuv(vec3 rgb)
{
    vec3 normalized = clamp(rgb / 255.0, vec3(0.0), vec3(1.0));
    vec3 yuv = vec3(
        dot(normalized, vec3(66.0, 129.0, 25.0)) + 16.0,
        dot(normalized, vec3(-38.0, -74.0, 112.0)) + 128.0,
        dot(normalized, vec3(112.0, -94.0, -18.0)) + 128.0);
    // Round the fixed-point conversion before storing each 8-bit video code.
    vec3 roundedCode = floor(yuv + vec3(0.5));
    return ivec3(clamp(roundedCode, vec3(0.0), vec3(255.0)));
}

void main()
{
    // Store packed 4:2:2 video bytes as one YUYV pair per texel.
    ivec2 videoPair = clamp(ivec2(gl_FragCoord.xy), ivec2(0), VideoPairSize - ivec2(1));
    ivec2 firstPixel = ivec2(videoPair.x * 2, videoPair.y);
    uvec3 firstColor = CopySceneColorToVideoFrame(firstPixel);
    uvec3 secondColor = CopySceneColorToVideoFrame(firstPixel + ivec2(1, 0));
    ivec3 firstYuv = ConvertRgbToYuv(vec3(firstColor));
    ivec3 secondYuv = ConvertRgbToYuv(vec3(secondColor));
    ivec3 pairChroma = ConvertRgbToYuv((vec3(firstColor) + vec3(secondColor)) * 0.5);
    outColor = vec4(float(firstYuv.x), float(pairChroma.y),
        float(secondYuv.x), float(pairChroma.z)) / 255.0;
}
