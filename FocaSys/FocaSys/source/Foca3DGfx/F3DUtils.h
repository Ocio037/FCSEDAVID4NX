#pragma once
#include<Foca3DGfx/Detail.h>
namespace Foca3DGfx
{
	namespace detail
	{
		f32 GetUvComponent(const FocaUvMappingTransform& transform, u32 component);
		void SetUvComponent(FocaUvMappingTransform& transform, u32 component, f32 value);
		f32 EaseOutBounce(f32 t);
		f32 EaseInOut(u8 mode, f32 t);
		f32 InterpolateUvComponent(const DynamicArray<FocaUvAnimationSample>& samples, size_t nodeIndex, u32 component, size_t sampleIndex0, size_t sampleIndex1, f32 t, u8 mode);
		u16 ReadU16(const DynamicArray<u8>& data, size_t offset);
		u32 ReadU32(const DynamicArray<u8>& data, size_t offset);
		String NormalizeTextureFormat(String format, const String& name);
		bool GuessRawDimensions(size_t pixelCount, u32& width, u32& height);
		bool HasTransparentVertex(const DynamicArray<FocaVertex>& vertices, const DynamicArray<u32>& indices);
		void CalculateSortCenter(const DynamicArray<FocaVertex>& vertices, const DynamicArray<u32>& indices, f32 sortCenter[3]);
		void DecodeBcColor(u16 color, u8& r, u8& g, u8& b);
		String Lowercase(String value);
	}
}
