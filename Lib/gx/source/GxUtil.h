#pragma once
#include<Windows.h>
#include<Array.h>
#include<DynamicArray.h>
#include<vulkan/vulkan.hpp>
#include<vulkan/vulkan_win32.h>
#include<Detail/Displayinfo.h>
#include<Detail/Detail.h>
namespace gx 
{
	namespace internal 
	{
		struct PhysicalDeviceData
		{
			vk::PhysicalDevice PhyDevice;
			detail::DisplayResolution Resolution;
		};
		struct DeviceData
		{
			vk::Device Device;
			vk::Queue Queue;
			vk::SwapchainKHR Swapchain;
			vk::Format ImageFormat;
			vk::Format SceneFormat = vk::Format::eB8G8R8A8Unorm;
			vk::Format DepthFormat = vk::Format::eUndefined;
			bool UseOffscreenScene = false;
			DynamicArray<vk::ImageView>ImagesViews;
			u32 GfxFamily;
			bool PresentWaitEnabled = false;
		};
		struct Device
		{
			PhysicalDeviceData PhyDeviceDat;
			DeviceData DeviceDat;
		};
	    u32 FindMemoryType(vk::PhysicalDevice physicalDevice, u32 typeFilter, vk::MemoryPropertyFlags properties);
		void WaitForDwmPresentIfAvailable();
		vk::SampleCountFlags GetImageSampleCounts(vk::PhysicalDevice physicalDevice, vk::Format format, vk::ImageUsageFlags usage);
		bool HasDeviceExtension(vk::PhysicalDevice physicalDevice, const char* extensionName);
		bool HasPresentableGraphicsQueue(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
		bool SupportsSingleSampleScene(vk::PhysicalDevice physicalDevice);
		bool HasColorAttachmentSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
		bool HasValidSpirvHeader(const void* shaderData, u32 shaderSize);
		vk::Format GetVkFormat(gx::TextureFormat format);
		u64 GetPixelCount(u32 width, u32 height);
		u32 GetCompressedBlockSize(vk::Format format);
		bool IsCompressed(gx::TextureFormat format);
		u64 GetSourceLayerSize(gx::TextureFormat format, vk::Format vkFormat, u32 width, u32 height);
		u64 GetUploadLayerSize(gx::TextureFormat format, vk::Format vkFormat, u32 width, u32 height);
		vk::ComponentMapping GetComponentMapping(gx::TextureFormat format);
		u8 Expand4To8(u8 value);
		void Expand4BitSingleChannelData(const u8* source, u8* target, u64 pixelCount);
		void ExpandLA4Data(const u8* source, u8* target, u64 pixelCount);
	}
}