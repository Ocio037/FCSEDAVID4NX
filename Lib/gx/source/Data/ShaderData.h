#pragma once
#include"../GxUtil.h"
#include<glm/mat4x4.hpp>
namespace gx 
{
	struct ShaderData
	{
		const internal::Device* DeviceContext = nullptr;
		vk::ShaderModule VertexModule{};
		vk::ShaderModule PixelModule{};
		vk::DescriptorSetLayout DescriptorSetLayout{};
		vk::PipelineLayout PipelineLayout{};
		vk::Pipeline OpaquePipeline{};
		vk::Pipeline TransparentPipeline{};
		vk::Pipeline OverlayPipeline{};
		glm::mat4 Mtx{ 1.0f };
		vk::Buffer TransformBuffer{};
		vk::DeviceMemory TransformMemory{};
		void* TransformMapped = nullptr;
	};
}
