#pragma once
#ifndef GX_COMMANDBUFFER_H_
#define GX_COMMANDBUFFER_H_
#include<Macros.h>
#include<Types.h>
#include<DynamicArray.h>
#include<MemTypes.h>
#include<vulkan/vulkan.hpp>
namespace gx {
	class Display;
	class Shader;
	class Buffer;
	class Texture;
	struct CommandBufferData;
	struct NativeFrameContext
	{
		vk::Device Device;
		vk::PhysicalDevice PhysicalDevice;
		vk::CommandBuffer CommandBuffer;
		vk::Extent2D Extent;
		vk::Extent2D SceneExtent;
		u32 SceneSampleCount = 1;
		vk::Format ColorFormat = vk::Format::eUndefined;
		u32 ImageIndex = 0;
		bool ScenePassActive = false;
		const DynamicArray<vk::ImageView>* SceneImageViews = nullptr;
		const DynamicArray<vk::ImageView>* OutputImageViews = nullptr;
	};
	class CommandBuffer : private NonCopy<>
	{
	public:
		CommandBuffer();
		explicit CommandBuffer(Display& display);
		~CommandBuffer();
		void Initialize(gx::Display& display);
		bool InitializeOffscreenScene(u32 width, u32 height, bool useSceneFormat = true);
		void Finalize();
		void WaitForPresent();
		void Render();
		void EndScenePass();
		[[nodiscard]] NativeFrameContext GetNativeFrameContext() const;
		void Submit();
		void WaitForGpu();
		void Present();
		void SetColor(f32 r, f32 g, f32 b, f32 a);
		void SetShader(Shader* shader);
		void PushFragmentConstants(const void* data, u32 sizeInBytes, u32 offsetInBytes = 0);
		void SetTransparencyPass(bool enabled);
		void SetOverlayPass(bool enabled);
		void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance);
		void BindVertexBuffer(Buffer* buffer);
		void BindIndexBuffer(Buffer* buffer);
		void BindTexture(Texture* texture);
		void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, s32 vertexOffset, u32 firstInstance);
	private:
		Mem::UniquePtr<CommandBufferData>Data;
	};
}
#endif // !GX_COMMANDBUFFER_H_
