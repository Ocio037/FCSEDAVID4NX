#include"Shader.h"
#include<Array.h>
#include"Data/ShaderData.h"
#include"GxUtil.h"
#include<Display.h>
#include<cstring>
#include<glm/gtc/type_ptr.hpp>
gx::Shader::Shader() : Data(nullptr)
{

}
gx::Shader::~Shader()
{
	Finalize();
}
void gx::Shader::Initialize(Display* display)
{
	Finalize();
	if (!display)
	{
		return;
	}
	Data = Mem::MakeUnique<ShaderData>();
	if (!Data)
	{
		return;
	}
	Data->DeviceContext = display->GetDeviceInfo();
}
void gx::Shader::Finalize()
{
	if (!Data)
	{
		return;
	}
	if (Data->DeviceContext && Data->DeviceContext->DeviceDat.Device)
	{
		vk::Device device = Data->DeviceContext->DeviceDat.Device;
		device.waitIdle();
		if (Data->OpaquePipeline)
		{
			device.destroyPipeline(Data->OpaquePipeline);
		}
		if (Data->TransparentPipeline)
		{
			device.destroyPipeline(Data->TransparentPipeline);
		}
		if (Data->OverlayPipeline)
		{
			device.destroyPipeline(Data->OverlayPipeline);
		}
		if (Data->PipelineLayout)
		{
			device.destroyPipelineLayout(Data->PipelineLayout);
		}
		if (Data->DescriptorSetLayout)
		{
			device.destroyDescriptorSetLayout(Data->DescriptorSetLayout);
		}
		if (Data->VertexModule)
		{
			device.destroyShaderModule(Data->VertexModule);
		}
		if (Data->PixelModule)
		{
			device.destroyShaderModule(Data->PixelModule);
		}
		if (Data->TransformMapped && Data->TransformMemory)
		{
			device.unmapMemory(Data->TransformMemory);
		}
		if (Data->TransformBuffer)
		{
			device.destroyBuffer(Data->TransformBuffer);
		}
		if (Data->TransformMemory)
		{
			device.freeMemory(Data->TransformMemory);
		}
	}
	Data.reset();
}
bool gx::Shader::Load(void* VertexShaderData, u32 VertexShaderSize, void* PixelShaderData, u32 PixelShaderSize)
{
	return Load(static_cast<const void*>(VertexShaderData), VertexShaderSize, static_cast<const void*>(PixelShaderData), PixelShaderSize, ShaderVertexLayout::Standard);
}
bool gx::Shader::Load(const void* VertexShaderData, u32 VertexShaderSize, const void* PixelShaderData, u32 PixelShaderSize, ShaderVertexLayout VertexLayout)
{
	if (!Data || !Data->DeviceContext || !Data->DeviceContext->DeviceDat.Device || !Data->DeviceContext->PhyDeviceDat.PhyDevice || !internal::HasValidSpirvHeader(VertexShaderData, VertexShaderSize) || !internal::HasValidSpirvHeader(PixelShaderData, PixelShaderSize))
	{
		return false;
	}
	try
	{
		vk::Device device = Data->DeviceContext->DeviceDat.Device;
		vk::ShaderModuleCreateInfo vertexModuleInfo{};
		vertexModuleInfo.setPCode(reinterpret_cast<const u32*>(VertexShaderData))
			.setCodeSize(VertexShaderSize);
		Data->VertexModule = device.createShaderModule(vertexModuleInfo);
		vk::ShaderModuleCreateInfo pixelModuleInfo{};
		pixelModuleInfo.setPCode(reinterpret_cast<const u32*>(PixelShaderData))
			.setCodeSize(PixelShaderSize);
		Data->PixelModule = device.createShaderModule(pixelModuleInfo);
		vk::PipelineShaderStageCreateInfo vertexStage = vk::PipelineShaderStageCreateInfo()
			.setModule(Data->VertexModule)
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex);
		vk::PipelineShaderStageCreateInfo pixelStage = vk::PipelineShaderStageCreateInfo()
			.setModule(Data->PixelModule)
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment);
		Array<vk::PipelineShaderStageCreateInfo, 2> stages = { vertexStage, pixelStage };
		const bool positionUvLayer = VertexLayout == ShaderVertexLayout::PositionUvLayer;
		vk::VertexInputBindingDescription bindingDesc = vk::VertexInputBindingDescription()
			.setBinding(0)
			.setStride(positionUvLayer ? sizeof(f32) * 6 : 48)
			.setInputRate(vk::VertexInputRate::eVertex);
		Array<vk::VertexInputAttributeDescription, 4> attrDescs{};
		Array<vk::VertexInputAttributeDescription, 2> positionUvLayerAttributes{};
		attrDescs[0] = vk::VertexInputAttributeDescription()
			.setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0);
		attrDescs[1] = vk::VertexInputAttributeDescription()
			.setBinding(0).setLocation(1).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(12);
		attrDescs[2] = vk::VertexInputAttributeDescription()
			.setBinding(0).setLocation(2).setFormat(vk::Format::eR32G32B32A32Sfloat).setOffset(24);
		attrDescs[3] = vk::VertexInputAttributeDescription()
			.setBinding(0).setLocation(3).setFormat(vk::Format::eR32G32Sfloat).setOffset(40);
		positionUvLayerAttributes[0] = attrDescs[0];
		positionUvLayerAttributes[1] = attrDescs[1];
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
			.setVertexBindingDescriptionCount(1)
			.setVertexBindingDescriptions(bindingDesc);
		if (positionUvLayer)
		{
			vertexInputInfo.setVertexAttributeDescriptions(positionUvLayerAttributes);
		}
		else
		{
			vertexInputInfo.setVertexAttributeDescriptions(attrDescs);
		}
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo()
			.setPrimitiveRestartEnable(VK_FALSE)
			.setTopology(vk::PrimitiveTopology::eTriangleList);
		vk::PipelineViewportStateCreateInfo viewportInfo = vk::PipelineViewportStateCreateInfo()
			.setScissorCount(1)
			.setViewportCount(1);
		vk::PipelineRasterizationStateCreateInfo rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setDepthBiasEnable(VK_FALSE)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setLineWidth(1.0f)
			.setPolygonMode(vk::PolygonMode::eFill)
			.setRasterizerDiscardEnable(VK_FALSE);
		vk::PipelineMultisampleStateCreateInfo multisampleInfo = vk::PipelineMultisampleStateCreateInfo()
			.setRasterizationSamples(vk::SampleCountFlagBits::e1)
			.setSampleShadingEnable(VK_FALSE)
			.setMinSampleShading(1.0f);
		const vk::ColorComponentFlags colorMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		vk::PipelineColorBlendAttachmentState opaqueColorBlend = vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(VK_FALSE)
			.setColorWriteMask(colorMask);
		vk::PipelineColorBlendAttachmentState transparentColorBlend = vk::PipelineColorBlendAttachmentState()
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setBlendEnable(VK_TRUE)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(colorMask)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
		vk::PipelineColorBlendStateCreateInfo opaqueColorBlendInfo = vk::PipelineColorBlendStateCreateInfo()
			.setAttachmentCount(1)
			.setAttachments(opaqueColorBlend)
			.setLogicOpEnable(VK_FALSE);
		vk::PipelineColorBlendStateCreateInfo transparentColorBlendInfo = vk::PipelineColorBlendStateCreateInfo()
			.setAttachmentCount(1)
			.setAttachments(transparentColorBlend)
			.setLogicOpEnable(VK_FALSE);
		vk::PipelineDepthStencilStateCreateInfo opaqueDepthState = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(VK_TRUE)
			.setDepthWriteEnable(VK_TRUE)
			.setDepthCompareOp(vk::CompareOp::eLess)
			.setDepthBoundsTestEnable(VK_FALSE)
			.setStencilTestEnable(VK_FALSE);
		vk::PipelineDepthStencilStateCreateInfo transparentDepthState = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(VK_TRUE)
			.setDepthWriteEnable(VK_FALSE)
			.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
			.setDepthBoundsTestEnable(VK_FALSE)
			.setStencilTestEnable(VK_FALSE);
		vk::PipelineDepthStencilStateCreateInfo overlayDepthState = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(VK_FALSE)
			.setDepthWriteEnable(VK_FALSE)
			.setDepthCompareOp(vk::CompareOp::eAlways)
			.setDepthBoundsTestEnable(VK_FALSE)
			.setStencilTestEnable(VK_FALSE);
		Array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicInfo = vk::PipelineDynamicStateCreateInfo()
			.setDynamicStateCount(static_cast<u32>(dynamicStates.size()))
			.setDynamicStates(dynamicStates);
		Array<vk::DescriptorSetLayoutBinding, 2> descriptorBindings{};
		descriptorBindings[0] = vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
		descriptorBindings[1] = vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);
		vk::DescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
		descriptorLayoutInfo.setBindings(descriptorBindings);
		Data->DescriptorSetLayout = device.createDescriptorSetLayout(descriptorLayoutInfo);
		vk::BufferCreateInfo transformBufferInfo{};
		transformBufferInfo.setSize(sizeof(f32) * 48)
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
			.setSharingMode(vk::SharingMode::eExclusive);
		Data->TransformBuffer = device.createBuffer(transformBufferInfo);
		const vk::MemoryRequirements transformMemoryRequirements = device.getBufferMemoryRequirements(Data->TransformBuffer);
		const u32 transformMemoryType = internal::FindMemoryType(Data->DeviceContext->PhyDeviceDat.PhyDevice, transformMemoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		if (transformMemoryType == UINT32_MAX)
		{
			Finalize();
			return false;
		}
		vk::MemoryAllocateInfo transformAllocation{};
		transformAllocation.setAllocationSize(transformMemoryRequirements.size)
			.setMemoryTypeIndex(transformMemoryType);
		Data->TransformMemory = device.allocateMemory(transformAllocation);
		device.bindBufferMemory(Data->TransformBuffer, Data->TransformMemory, 0);
		Data->TransformMapped = device.mapMemory(Data->TransformMemory, 0, sizeof(f32) * 48);
		if (!Data->TransformMapped)
		{
			Finalize();
			return false;
		}
		std::memset(Data->TransformMapped, 0, sizeof(f32) * 48);
		vk::PushConstantRange fontColorRange = vk::PushConstantRange()
			.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			.setOffset(0)
			.setSize(sizeof(f32) * 4);
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.setSetLayouts(Data->DescriptorSetLayout)
			.setPushConstantRanges(fontColorRange);
		Data->PipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);
		const bool useOffscreenScene = Data->DeviceContext->DeviceDat.UseOffscreenScene;
		const vk::Format sceneColorFormat = useOffscreenScene
			? Data->DeviceContext->DeviceDat.SceneFormat
			: Data->DeviceContext->DeviceDat.ImageFormat;
		vk::AttachmentDescription colorAttachment = vk::AttachmentDescription()
			.setFormat(sceneColorFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(useOffscreenScene ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::ePresentSrcKHR);
		vk::AttachmentDescription depthAttachment = vk::AttachmentDescription()
			.setFormat(Data->DeviceContext->DeviceDat.DepthFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		vk::AttachmentReference colorRef = vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
		vk::AttachmentReference depthRef = vk::AttachmentReference()
			.setAttachment(1)
			.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		vk::SubpassDescription subPass = vk::SubpassDescription()
			.setColorAttachmentCount(1)
			.setColorAttachments(colorRef)
			.setPDepthStencilAttachment(&depthRef)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		const Array<vk::AttachmentDescription, 2> renderPassAttachments = { colorAttachment, depthAttachment };
		vk::RenderPassCreateInfo renderPassInfo = vk::RenderPassCreateInfo()
			.setAttachments(renderPassAttachments)
			.setSubpasses(subPass);
		vk::RenderPass tempRenderPass = device.createRenderPass(renderPassInfo);
		vk::GraphicsPipelineCreateInfo pipelineInfo = vk::GraphicsPipelineCreateInfo()
			.setBasePipelineHandle(nullptr)
			.setPDynamicState(&dynamicInfo)
			.setPInputAssemblyState(&inputAssemblyInfo)
			.setPMultisampleState(&multisampleInfo)
			.setPRasterizationState(&rasterizationInfo)
			.setPDepthStencilState(&opaqueDepthState)
			.setStageCount(static_cast<u32>(stages.size()))
			.setStages(stages)
			.setPVertexInputState(&vertexInputInfo)
			.setPViewportState(&viewportInfo)
			.setPColorBlendState(&opaqueColorBlendInfo)
			.setSubpass(0)
			.setLayout(Data->PipelineLayout)
			.setRenderPass(tempRenderPass);
		Data->OpaquePipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;
		pipelineInfo.setPColorBlendState(&transparentColorBlendInfo)
			.setPDepthStencilState(&transparentDepthState);
		Data->TransparentPipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;
		pipelineInfo.setPDepthStencilState(&overlayDepthState);
		Data->OverlayPipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;
		device.destroyRenderPass(tempRenderPass);
	}
	catch (...)
	{
		Finalize();
		return false;
	}
	return Data && Data->OpaquePipeline && Data->TransparentPipeline && Data->OverlayPipeline && Data->TransformMapped;
}
void gx::Shader::SetMatrices(const glm::mat4& Mtx, const glm::mat4& Projection, const glm::mat4& World)
{
	if (!Data || !Data->TransformMapped)
	{
		return;
	}
	f32* matrices = static_cast<f32*>(Data->TransformMapped);
	Data->Mtx = Mtx;
	std::memcpy(matrices, glm::value_ptr(Mtx), sizeof(glm::mat4));
	std::memcpy(matrices + 16, glm::value_ptr(Projection), sizeof(glm::mat4));
	std::memcpy(matrices + 32, glm::value_ptr(World), sizeof(glm::mat4));
}
const glm::mat4& gx::Shader::GetMtx() const
{
	static const glm::mat4 identity(1.0f);
	return Data ? Data->Mtx : identity;
}