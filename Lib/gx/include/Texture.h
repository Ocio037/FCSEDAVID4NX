#pragma once
#ifndef GX_TEXTURE_H_
#define GX_TEXTURE_H_
#include<Macros.h>
#include<Types.h>
#include<MemTypes.h>
#include<Detail/Detail.h>
namespace gx {
	
	enum class TextureFilter
	{
		Nearest,
		Linear
	};
	class Display;
	class Shader;
	class CommandBuffer;
	struct TextureData;
	class Texture final : private NonCopy<>
	{
		friend class CommandBuffer;
	public:
		Texture();
		virtual ~Texture();
		bool Initialize(Display& display, Shader& shader, u32 width, u32 height, const u8* rgbaPixels);
		bool InitializeArray(Display& display, Shader& shader, u32 width, u32 height, u32 layers, TextureFilter filter = TextureFilter::Nearest, TextureFormat format = TextureFormat::Rgba8Unorm);
		bool UploadLayer(const void* pixelData, u64 sizeInBytes, u32 layer);
		void Finalize();
		[[nodiscard]] bool IsValid() const;
	private:
		bool InitializeImpl(Display& display, Shader& shader, u32 width, u32 height, u32 layers, TextureFilter filter, TextureFormat format, bool isArray);
		Mem::UniquePtr<TextureData> Data;
	};
}
#endif // !GX_TEXTURE_H_
