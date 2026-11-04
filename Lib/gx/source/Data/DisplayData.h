#pragma once
#include"../GxUtil.h"
namespace gx
{
	struct DisplayData
	{
		vk::Extent2D Extent;
		vk::Instance Instance;
		VkSurfaceKHR Surface;
		DynamicArray<vk::Image>Images;
		u32 ImageCount;
		internal::Device DeviceContext;
	};
}
