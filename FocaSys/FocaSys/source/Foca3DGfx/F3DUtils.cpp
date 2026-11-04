#include"F3DUtils.h"
#include<algorithm>
#include<cmath>
f32 Foca3DGfx::detail::GetUvComponent(const Foca3DGfx::detail::FocaUvMappingTransform& transform, u32 component)
{
	if (component < 3)
	{
		return transform.Location[component];
	}
	if (component < 6)
	{
		return transform.Rotation[component - 3];
	}
	return transform.Scale[component - 6];
}
void Foca3DGfx::detail::SetUvComponent(Foca3DGfx::detail::FocaUvMappingTransform& transform, u32 component, f32 value)
{
	if (component < 3)
	{
		transform.Location[component] = value;
	}
	else if (component < 6)
	{
		transform.Rotation[component - 3] = value;
	}
	else
	{
		transform.Scale[component - 6] = value;
	}
}
f32 Foca3DGfx::detail::EaseOutBounce(f32 t)
{
	constexpr f32 n = 7.5625f;
	constexpr f32 d = 2.75f;
	if (t < 1.0f / d)
	{
		return n * t * t;
	}
	if (t < 2.0f / d)
	{
		t -= 1.5f / d;
		return n * t * t + 0.75f;
	}
	if (t < 2.5f / d)
	{
		t -= 2.25f / d;
		return n * t * t + 0.9375f;
	}
	t -= 2.625f / d;
	return n * t * t + 0.984375f;
}
f32 Foca3DGfx::detail::EaseInOut(u8 mode, f32 t)
{
	constexpr f32 pi = 3.14159265358979323846f;
	switch (mode)
	{
	case 3: 
		return 0.5f - 0.5f * std::cos(pi * t);
	case 4: 
		return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
	case 5: 
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	case 6: 
		return t < 0.5f ? 8.0f * std::pow(t, 4.0f) : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) * 0.5f;
	case 7: 
		return t < 0.5f ? 16.0f * std::pow(t, 5.0f) : 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) * 0.5f;
	case 8:
	{
		if (t == 0.0f || t == 1.0f)
		{
			return t;
		}
		return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
	}
	case 9:
		return t < 0.5f ? (1.0f - std::sqrt(1.0f - std::pow(2.0f * t, 2.0f))) * 0.5f : (1.0f + std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f))) * 0.5f;
	case 10:
	{
		constexpr f32 c = 1.70158f * 1.525f;
		return t < 0.5f ? std::pow(2.0f * t, 2.0f) * ((c + 1.0f) * 2.0f * t - c) * 0.5f : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c + 1.0f) * (t * 2.0f - 2.0f) + c) + 2.0f) * 0.5f;
	}
	case 11:
		return t < 0.5f ? (1.0f - EaseOutBounce(1.0f - 2.0f * t)) * 0.5f : (1.0f + EaseOutBounce(2.0f * t - 1.0f)) * 0.5f;
	case 12:
	{
		if (t == 0.0f || t == 1.0f)
		{
			return t;
		}
		constexpr f32 c = (2.0f * pi) / 4.5f;
		return t < 0.5f ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c)) * 0.5f : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c)) * 0.5f + 1.0f; // ELASTIC
	}
	default:
		return t;
	}
}
f32 Foca3DGfx::detail::InterpolateUvComponent(const DynamicArray<Foca3DGfx::detail::FocaUvAnimationSample>& samples, size_t nodeIndex, u32 component, size_t sampleIndex0, size_t sampleIndex1, f32 t, u8 mode)
{
	const f32 a = GetUvComponent(samples[sampleIndex0].MappingTransforms[nodeIndex], component);
	const f32 b = GetUvComponent(samples[sampleIndex1].MappingTransforms[nodeIndex], component);
	if (mode == 0)
	{
		return a;
	}
	if (mode == 1)
	{
		return a + (b - a) * t; // LINEAR
	}
	if (mode == 2)
	{
		const size_t previous = (sampleIndex0 + samples.size() - 1) % samples.size();
		const size_t next = (sampleIndex1 + 1) % samples.size();
		const f32 before = GetUvComponent(samples[previous].MappingTransforms[nodeIndex], component);
		const f32 after = GetUvComponent(samples[next].MappingTransforms[nodeIndex], component);
		const f32 tangent0 = (b - before) * 0.5f;
		const f32 tangent1 = (after - a) * 0.5f;
		const f32 t2 = t * t;
		const f32 t3 = t2 * t;
		return (2.0f * t3 - 3.0f * t2 + 1.0f) * a + (t3 - 2.0f * t2 + t) * tangent0 + (-2.0f * t3 + 3.0f * t2) * b + (t3 - t2) * tangent1;
	}
	return a + (b - a) * EaseInOut(mode, t);
}
u16 Foca3DGfx::detail::ReadU16(const DynamicArray<u8>& data, size_t offset)
{
	return static_cast<u16>(data[offset]) | (static_cast<u16>(data[offset + 1]) << 8);
}
u32 Foca3DGfx::detail::ReadU32(const DynamicArray<u8>& data, size_t offset)
{
	return static_cast<u32>(data[offset]) | (static_cast<u32>(data[offset + 1]) << 8) | (static_cast<u32>(data[offset + 2]) << 16) | (static_cast<u32>(data[offset + 3]) << 24);
}
String Foca3DGfx::detail::NormalizeTextureFormat(String format, const String& name)
{
	if (format.empty())
	{
		const size_t dot = name.find_last_of('.');
		if (dot != String::npos)
		{
			format = name.substr(dot + 1);
		}
	}
	if (!format.empty() && format.front() == '.')
	{
		format.erase(format.begin());
	}
	std::transform(format.begin(), format.end(), format.begin(), [](unsigned char c)
	{
		return static_cast<char>(std::tolower(c));
	});
	return format;
}
bool Foca3DGfx::detail::GuessRawDimensions(size_t pixelCount, u32& width, u32& height)
{
	if (pixelCount == 0)
	{
		return false;
	}
	size_t guessedWidth = static_cast<size_t>(std::sqrt(static_cast<double>(pixelCount)));
	if (guessedWidth * guessedWidth != pixelCount)
	{
		const size_t h2 = static_cast<size_t>(std::sqrt(static_cast<double>(pixelCount / 2)));
		const size_t w2 = h2 * 2;
		const size_t h1 = static_cast<size_t>(std::sqrt(static_cast<double>(pixelCount / 2)));
		const size_t w1 = h1;
		const size_t h4 = static_cast<size_t>(std::sqrt(static_cast<double>(pixelCount / 4)));
		const size_t w4 = h4 * 4;
		const size_t h1x4 = static_cast<size_t>(std::sqrt(static_cast<double>(pixelCount / 4)));
		const size_t w1x4 = h1x4;
		if (w2 * h2 == pixelCount)
		{
			guessedWidth = w2;
		}
		else if (w1 * (w1 * 2) == pixelCount)
		{
			guessedWidth = w1;
		}
		else if (w4 * h4 == pixelCount)
		{
			guessedWidth = w4;
		}
		else if (w1x4 * (w1x4 * 4) == pixelCount)
		{
			guessedWidth = w1x4;
		}
	}
	if (guessedWidth * guessedWidth != pixelCount)
	{
		const size_t scale43 = static_cast<size_t>(std::sqrt(static_cast<double>(pixelCount / 12)));
		if (scale43 > 0 && scale43 * scale43 * 12 == pixelCount)
		{
			width = static_cast<u32>(scale43 * 4);
			height = static_cast<u32>(scale43 * 3);
			return true;
		}
	}
	if (guessedWidth == 0 || pixelCount % guessedWidth != 0 || guessedWidth > UINT32_MAX || pixelCount / guessedWidth > UINT32_MAX)
	{
		return false;
	}
	width = static_cast<u32>(guessedWidth);
	height = static_cast<u32>(pixelCount / guessedWidth);
	return true;
}
bool Foca3DGfx::detail::HasTransparentVertex(const DynamicArray<Foca3DGfx::detail::FocaVertex>& vertices, const DynamicArray<u32>& indices)
{
	if (indices.empty())
	{
		return std::any_of(vertices.begin(), vertices.end(), [](const auto& vertex) {
			return vertex.Color[3] < 0.999f;
		});
	}
	return std::any_of(indices.begin(), indices.end(), [&](u32 index)
	{
		return index < vertices.size() && vertices[index].Color[3] < 0.999f;
	});
}
void Foca3DGfx::detail::CalculateSortCenter(const DynamicArray<Foca3DGfx::detail::FocaVertex>& vertices, const DynamicArray<u32>& indices, f32 sortCenter[3])
{
	sortCenter[0] = sortCenter[1] = sortCenter[2] = 0.0f;
	size_t count = 0;
	if (indices.empty())
	{
		for (const auto& vertex : vertices)
		{
			for (u32 axis = 0; axis < 3; ++axis)
			{
				sortCenter[axis] += vertex.Position[axis];
			}
			++count;
		}
	}
	else
	{
		for (u32 index : indices)
		{
			if (index >= vertices.size())
			{
				continue;
			}
			for (u32 axis = 0; axis < 3; ++axis)
			{
				sortCenter[axis] += vertices[index].Position[axis];
			}
			++count;
		}
	}
	if (count != 0)
	{
		for (u32 axis = 0; axis < 3; ++axis)
		{
			sortCenter[axis] /= static_cast<f32>(count);
		}
	}
}
void Foca3DGfx::detail::DecodeBcColor(u16 color, u8& r, u8& g, u8& b)
{
	r = static_cast<u8>(((color >> 11) & 0x1f) * 255 / 31);
	g = static_cast<u8>(((color >> 5) & 0x3f) * 255 / 63);
	b = static_cast<u8>((color & 0x1f) * 255 / 31);
}
String Foca3DGfx::detail::Lowercase(String value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}
