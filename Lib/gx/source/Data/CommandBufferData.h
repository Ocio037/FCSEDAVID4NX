#pragma once
#include"../GxUtil.h"
namespace gx
{
	class Shader;
	struct CommandBufferData
	{
		vk::Device Device;
		vk::PhysicalDevice PhysicalDevice;
		vk::Queue Queue;
		vk::SwapchainKHR Swapchain;
		vk::Format ImageFormat = vk::Format::eUndefined;
		vk::Format SceneFormat = vk::Format::eUndefined;
		vk::Format DepthFormat = vk::Format::eUndefined;
		DynamicArray<vk::ImageView> SwapchainImageViews;
		vk::CommandPool CommandPool;
		vk::CommandBuffer CommandBuffer;
		vk::RenderPass RenderPass;
		DynamicArray<vk::Framebuffer> Framebuffers;
		DynamicArray<vk::Image> SceneImages;
		DynamicArray<vk::DeviceMemory> SceneMemories;
		DynamicArray<vk::ImageView> SceneImageViews;
		vk::Semaphore ImageAvailableSemaphore;
		vk::Semaphore RenderFinishedSemaphore;
		vk::Fence InFlightFence;
		std::uint64_t NextPresentId = 1;
		std::uint64_t PendingPresentId = 0;
		PFN_vkWaitForPresentKHR WaitForPresent = nullptr;
		Array<vk::ClearValue, 3> Clear;
		vk::PipelineLayout PipelineLayout;
		vk::Pipeline CurrentPipeline;
		Shader* CurrentShader = nullptr;
		vk::Extent2D Extent;
		vk::Extent2D SceneExtent;
		vk::Image DepthImage;
		vk::DeviceMemory DepthMemory;
		vk::ImageView DepthImageView;
		u32 RenderPassAttachmentCount = 0;
		bool OffscreenSceneEnabled = false;
		bool ScenePassActive = false;
		bool TransparencyPass = false;
		bool OverlayPass = false;
		bool Submitted = false;
		bool ImageAcquired = false;
		bool PresentWaitEnabled = false;
		bool PresentationPending = false;
		u32 ImageIndex = 0;
	};
}
