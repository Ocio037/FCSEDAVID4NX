#pragma once
#include"../GxUtil.h"
namespace gx 
{
	struct BufferData
	{
		vk::Device Device;
		vk::PhysicalDevice PhyDevice;
		vk::Buffer VertexBuffer;
		vk::DeviceMemory VertexMemory;
		u64 VertexSize = 0;
		vk::Buffer IndexBuffer;
		vk::DeviceMemory IndexMemory;
		u32 IndexCount = 0;
		bool HasVertex = false;
		bool HasIndex = false;
	};
}
