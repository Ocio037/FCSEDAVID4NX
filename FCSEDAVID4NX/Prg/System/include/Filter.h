#pragma once
#include<CommandBuffer.h>
#include<MemTypes.h>
#include<Types.h>
#include<vulkan/vulkan.hpp>
#include<DynamicArray.h>
#include<array>
namespace Game
{
	class Filter final : private NonCopy<>
	{
	public:
		Filter();
		~Filter();
		bool Initialize(gx::CommandBuffer& commandBuffer, const void* vertexShaderData, u32 vertexShaderSize, const void* resolveFragmentShaderData, u32 resolveFragmentShaderSize, const void* presentFragmentShaderData, u32 presentFragmentShaderSize, u32 sceneWidth, u32 sceneHeight);
		void SetCopyFilter(const std::array<u8, 7>& coefficients);
		void Record(const gx::NativeFrameContext& frame);
		void Finalize();
	private:
		vk::Device Device;
		vk::RenderPass RenderPass;
		DynamicArray<vk::Framebuffer> Framebuffers;
		vk::RenderPass ResolveRenderPass;
		DynamicArray<vk::Framebuffer> ResolveFramebuffers;
		DynamicArray<vk::Image> VideoFrameImages;
		DynamicArray<vk::DeviceMemory> VideoFrameMemories;
		DynamicArray<vk::ImageView> VideoFrameImageViews;
		vk::ShaderModule VertexModule;
		vk::ShaderModule ResolveFragmentModule;
		vk::ShaderModule PresentFragmentModule;
		vk::DescriptorSetLayout DescriptorSetLayout;
		vk::DescriptorPool DescriptorPool;
		DynamicArray<vk::DescriptorSet> ResolveDescriptorSets;
		DynamicArray<vk::DescriptorSet> DescriptorSets;
		vk::PipelineLayout PipelineLayout;
		vk::Pipeline Pipeline;
		vk::Pipeline ResolvePipeline;
		vk::Sampler Sampler;
		std::array<u8, 7> CopyFilterTaps;
	};
}
