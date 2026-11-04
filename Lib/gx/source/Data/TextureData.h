#pragma once
#include"../GxUtil.h"
#include<Texture.h>
namespace gx 
{
	struct TextureData
	{
		vk::Device Device;
		vk::PhysicalDevice PhysicalDevice;
		vk::Queue Queue;
		u32 QueueFamily = UINT32_MAX;
		vk::Image Image;
		vk::DeviceMemory ImageMemory;
		vk::ImageView ImageView;
		vk::Sampler Sampler;
		vk::DescriptorPool DescriptorPool;
		vk::DescriptorSet DescriptorSet;
		u32 Width = 0;
		u32 Height = 0;
		u32 Layers = 0;
		TextureFormat Format = TextureFormat::Rgba8Unorm;
		vk::Format VkFormat = vk::Format::eUndefined;
		bool IsArray = false;
		DynamicArray<bool> UploadedLayers;
	};
}
