#pragma once
#include<Macros.h>
#include<Types.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include<glm/glm.hpp>
#include<Buffer.h>
#include<CommandBuffer.h>
#include<Display.h>
#include<Shader.h>
#include<Texture.h>
#include<DynamicArray.h>
#include<DynamicString.h>
namespace Font
{
	struct VertexData
	{
		f32 Pos[3]{};
		f32 Uv[3]{};
	};
	static_assert(sizeof(VertexData) == sizeof(f32) * 6);
	struct AtlasInfo
	{
		u16 Height = 0;
		u16 Width = 0;
		u16 GridCols = 0;
		u16 GridRows = 0;
		gx::TextureFormat Format = gx::TextureFormat::Rgba8Unorm;
	};
	class font : NonCopy<>
	{
	public:
		font();
		explicit font(gx::Display& display, gx::CommandBuffer& commandBuffer);
		~font();
		void Create(gx::Display& display, gx::CommandBuffer& commandBuffer);
		void Destroy();
		void Reset();
		void Update();
		void SetAtlasInfo(AtlasInfo info);
		void LoadCharsetMap(const char16* buffer, uptr numChars);
		void LoadShader(const char* vertexShader, u32 vertexShaderSize, const char* fragmentShader, u32 fragmentShaderSize, bool transparent = true);
		void LoadAtlas(const void* imageData, uptr size, u32 layer);
		void FinAtlasLoad();
		void SetTextColor(f32 r, f32 g, f32 b, f32 a);
		void DrawTextU(const char16* wstr, f32 x, f32 y, f32 charW, f32 charH);
		void Draw(const char16* wstr, f32 x, f32 y, f32 charW, f32 charH);
		void Draw(f32 x, f32 y, f32 charW, f32 charH, const char16* format, ...);
		void RectDrawer();
		void SetColor(f32 r, f32 g, f32 b, f32 a);
		[[nodiscard]] bool IsValid() const;
	private:
		void InitializeDefaultCharset();
		void RebuildAtlasTexture();
		[[nodiscard]] u32 GetAtlasLayerCount() const;
		gx::Display* Display = nullptr;
		gx::CommandBuffer* CmdBuffer = nullptr;
		gx::Shader Shader;
		gx::Texture Textura;
		gx::Buffer VertexBuffer;
		DynamicArray<VertexData> Vertices;
		DynamicArray<s32> CharsetLUT;
		DynamicArray<bool> AtlasLayersLoaded;
		AtlasInfo Info{};
		char16 CharsetBase = 0;
		u32 CharsetCount = 0;
		u32 GridCols = 0;
		u32 GridRows = 0;
		u32 currentVertexCount = 0;
		u32 textCount = 0;
		glm::vec4 TextColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		bool HasAtlasInfo = false;
		bool ShaderLoaded = false;
		bool AtlasLoaded = false;
		bool Transparent = true;
	};
}