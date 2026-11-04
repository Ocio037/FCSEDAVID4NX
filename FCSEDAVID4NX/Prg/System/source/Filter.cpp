#include<Filter.h>
#include<algorithm>
#include<array>
#include<cstring>
namespace
{
	constexpr u32 VideoSignalWidth = 640;
	constexpr u32 VideoPairWidth = VideoSignalWidth / 2;
	constexpr u32 VideoFrameHeight = 480;
	bool HasValidSpirv(const void* shaderData, u32 shaderSize)
	{
		static constexpr std::array<u8, 4> magic = { 0x03, 0x02, 0x23, 0x07 };
		return shaderData && shaderSize >= magic.size() && shaderSize % sizeof(u32) == 0 && std::memcmp(shaderData, magic.data(), magic.size()) == 0;
	}
	u32 FindMemoryType(vk::PhysicalDevice physicalDevice, u32 typeFilter, vk::MemoryPropertyFlags properties)
	{
		const vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
		for (u32 i = 0; i < memoryProperties.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		return UINT32_MAX;
	}
}
Game::Filter::Filter() : 
	Device(),
	RenderPass(),
	ResolveRenderPass(),
	VertexModule(),
	ResolveFragmentModule(),
	PresentFragmentModule(),
	DescriptorSetLayout(),
	DescriptorPool(),
	PipelineLayout(),
	Pipeline(),
	ResolvePipeline(),
	Sampler(),
	CopyFilterTaps({ 4, 8, 12, 16, 12, 8, 4 })
{

}
Game::Filter::~Filter()
{
	Finalize();
}
bool Game::Filter::Initialize(gx::CommandBuffer& commandBuffer, const void* vertexShaderData, u32 vertexShaderSize, const void* resolveFragmentShaderData, u32 resolveFragmentShaderSize, const void* presentFragmentShaderData, u32 presentFragmentShaderSize, u32 sceneWidth, u32 sceneHeight)
{
	Finalize();
	if (!HasValidSpirv(vertexShaderData, vertexShaderSize) || !HasValidSpirv(resolveFragmentShaderData, resolveFragmentShaderSize) || !HasValidSpirv(presentFragmentShaderData, presentFragmentShaderSize) || !commandBuffer.InitializeOffscreenScene(sceneWidth, sceneHeight, true))
	{
		return false;
	}
	const gx::NativeFrameContext frame = commandBuffer.GetNativeFrameContext();
	if (!frame.Device || !frame.PhysicalDevice || frame.SceneSampleCount != 1 || !frame.SceneImageViews || !frame.OutputImageViews || frame.SceneImageViews->empty() || frame.SceneImageViews->size() != frame.OutputImageViews->size() || frame.Extent.width < VideoSignalWidth || frame.Extent.height < VideoFrameHeight)
	{
		return false;
	}
	Device = frame.Device;
	try
	{
		Device.waitIdle();
		const std::size_t imageCount = frame.SceneImageViews->size();
		const vk::Format videoFrameFormat = vk::Format::eR8G8B8A8Unorm;
		const vk::FormatProperties videoFrameFormatProperties = frame.PhysicalDevice.getFormatProperties(videoFrameFormat);
		const vk::FormatFeatureFlags requiredVideoFrameFeatures = vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage;
		if ((videoFrameFormatProperties.optimalTilingFeatures & requiredVideoFrameFeatures) != requiredVideoFrameFeatures)
		{
			Finalize();
			return false;
		}
		VideoFrameImages.resize(imageCount);
		VideoFrameMemories.resize(imageCount);
		VideoFrameImageViews.resize(imageCount);
		for (std::size_t i = 0; i < imageCount; ++i)
		{
			vk::ImageCreateInfo imageInfo = {};
			imageInfo.setImageType(vk::ImageType::e2D)
				.setFormat(videoFrameFormat)
				.setExtent(vk::Extent3D(VideoPairWidth, VideoFrameHeight, 1))
				.setMipLevels(1)
				.setArrayLayers(1)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setTiling(vk::ImageTiling::eOptimal)
				.setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
				.setSharingMode(vk::SharingMode::eExclusive)
				.setInitialLayout(vk::ImageLayout::eUndefined);
			VideoFrameImages[i] = Device.createImage(imageInfo);
			const vk::MemoryRequirements memoryRequirements = Device.getImageMemoryRequirements(VideoFrameImages[i]);
			const u32 memoryType = FindMemoryType(frame.PhysicalDevice, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
			if (memoryType == UINT32_MAX)
			{
				Finalize();
				return false;
			}
			vk::MemoryAllocateInfo allocation = {};
			allocation.setAllocationSize(memoryRequirements.size).setMemoryTypeIndex(memoryType);
			VideoFrameMemories[i] = Device.allocateMemory(allocation);
			Device.bindImageMemory(VideoFrameImages[i], VideoFrameMemories[i], 0);
			vk::ImageViewCreateInfo viewInfo = {};
			viewInfo.setImage(VideoFrameImages[i])
				.setViewType(vk::ImageViewType::e2D)
				.setFormat(videoFrameFormat)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			VideoFrameImageViews[i] = Device.createImageView(viewInfo);
		}
		const vk::AttachmentDescription resolveColorAttachment = vk::AttachmentDescription()
			.setFormat(videoFrameFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		const vk::AttachmentReference resolveColorReference = vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
		const vk::SubpassDescription resolveSubpass = vk::SubpassDescription()
			.setColorAttachmentCount(1)
			.setColorAttachments(resolveColorReference)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		vk::RenderPassCreateInfo resolveRenderPassInfo{};
		resolveRenderPassInfo.setAttachments(resolveColorAttachment).setSubpasses(resolveSubpass);
		ResolveRenderPass = Device.createRenderPass(resolveRenderPassInfo);
		ResolveFramebuffers.reserve(imageCount);
		for (vk::ImageView videoFrameView : VideoFrameImageViews)
		{
			const std::array<vk::ImageView, 1> attachments = { videoFrameView };
			vk::FramebufferCreateInfo framebufferInfo{};
			framebufferInfo.setRenderPass(ResolveRenderPass)
				.setAttachments(attachments)
				.setWidth(VideoPairWidth)
				.setHeight(VideoFrameHeight)
				.setLayers(1);
			ResolveFramebuffers.push_back(Device.createFramebuffer(framebufferInfo));
		}
		const vk::AttachmentDescription outputColorAttachment = vk::AttachmentDescription()
			.setFormat(frame.ColorFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
		const vk::AttachmentReference outputColorReference = vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
		const vk::SubpassDescription outputSubpass = vk::SubpassDescription()
			.setColorAttachmentCount(1)
			.setColorAttachments(outputColorReference)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		const vk::SubpassDependency outputDependency = vk::SubpassDependency()
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		vk::RenderPassCreateInfo outputRenderPassInfo{};
		outputRenderPassInfo.setAttachments(outputColorAttachment)
			.setSubpasses(outputSubpass)
			.setDependencies(outputDependency);
		RenderPass = Device.createRenderPass(outputRenderPassInfo);
		Framebuffers.reserve(imageCount);
		for (vk::ImageView outputView : *frame.OutputImageViews)
		{
			const std::array<vk::ImageView, 1> attachments = { outputView };
			vk::FramebufferCreateInfo framebufferInfo{};
			framebufferInfo.setRenderPass(RenderPass)
				.setAttachments(attachments)
				.setWidth(frame.Extent.width)
				.setHeight(frame.Extent.height)
				.setLayers(1);
			Framebuffers.push_back(Device.createFramebuffer(framebufferInfo));
		}
		const vk::DescriptorSetLayoutBinding samplerBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
		vk::DescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
		descriptorLayoutInfo.setBindings(samplerBinding);
		DescriptorSetLayout = Device.createDescriptorSetLayout(descriptorLayoutInfo);
		const vk::SamplerCreateInfo samplerInfo = vk::SamplerCreateInfo()
			.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setMipmapMode(vk::SamplerMipmapMode::eNearest)
			.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
			.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
			.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
			.setMaxLod(0.0f);
		Sampler = Device.createSampler(samplerInfo);
		const u32 descriptorSetCount = static_cast<u32>(imageCount * 2);
		const vk::DescriptorPoolSize poolSize = vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(descriptorSetCount);
		vk::DescriptorPoolCreateInfo poolInfo{};
		poolInfo.setMaxSets(descriptorSetCount).setPoolSizes(poolSize);
		DescriptorPool = Device.createDescriptorPool(poolInfo);
		DynamicArray<vk::DescriptorSetLayout> layouts(descriptorSetCount, DescriptorSetLayout);
		vk::DescriptorSetAllocateInfo descriptorAllocation{};
		descriptorAllocation.setDescriptorPool(DescriptorPool).setSetLayouts(layouts);
		DynamicArray<vk::DescriptorSet> descriptorSets(descriptorSetCount);
		if (Device.allocateDescriptorSets(&descriptorAllocation, descriptorSets.data()) != vk::Result::eSuccess)
		{
			Finalize();
			return false;
		}
		ResolveDescriptorSets.assign(descriptorSets.begin(), descriptorSets.begin() + imageCount);
		DescriptorSets.assign(descriptorSets.begin() + imageCount, descriptorSets.end());
		for (std::size_t i = 0; i < imageCount; ++i)
		{
			const vk::DescriptorImageInfo resolveImageInfo = vk::DescriptorImageInfo()
				.setSampler(Sampler)
				.setImageView((*frame.SceneImageViews)[i])
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			const vk::WriteDescriptorSet resolveWrite = vk::WriteDescriptorSet()
				.setDstSet(ResolveDescriptorSets[i])
				.setDstBinding(0)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(resolveImageInfo);
			Device.updateDescriptorSets(resolveWrite, {});
			const vk::DescriptorImageInfo presentImageInfo = vk::DescriptorImageInfo()
				.setSampler(Sampler)
			.setImageView(VideoFrameImageViews[i])
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			const vk::WriteDescriptorSet presentWrite = vk::WriteDescriptorSet()
				.setDstSet(DescriptorSets[i])
				.setDstBinding(0)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(presentImageInfo);
			Device.updateDescriptorSets(presentWrite, {});
		}
		vk::ShaderModuleCreateInfo vertexModuleInfo{};
		vertexModuleInfo.setPCode(reinterpret_cast<const u32*>(vertexShaderData)).setCodeSize(vertexShaderSize);
		VertexModule = Device.createShaderModule(vertexModuleInfo);
		vk::ShaderModuleCreateInfo resolveFragmentModuleInfo{};
		resolveFragmentModuleInfo.setPCode(reinterpret_cast<const u32*>(resolveFragmentShaderData))
			.setCodeSize(resolveFragmentShaderSize);
		ResolveFragmentModule = Device.createShaderModule(resolveFragmentModuleInfo);
		vk::ShaderModuleCreateInfo presentFragmentModuleInfo{};
		presentFragmentModuleInfo.setPCode(reinterpret_cast<const u32*>(presentFragmentShaderData))
			.setCodeSize(presentFragmentShaderSize);
		PresentFragmentModule = Device.createShaderModule(presentFragmentModuleInfo);
		std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo().setModule(VertexModule).setPName("main").setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo().setModule(ResolveFragmentModule).setPName("main").setStage(vk::ShaderStageFlagBits::eFragment)
		};
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
		vk::PipelineViewportStateCreateInfo viewportInfo{};
		viewportInfo.setViewportCount(1).setScissorCount(1);
		vk::PipelineRasterizationStateCreateInfo rasterizationInfo{};
		rasterizationInfo.setPolygonMode(vk::PolygonMode::eFill)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setLineWidth(1.0f);
		vk::PipelineMultisampleStateCreateInfo multisampleInfo{};
		multisampleInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);
		const vk::PipelineColorBlendAttachmentState blendAttachment = vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(VK_FALSE)
			.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		vk::PipelineColorBlendStateCreateInfo blendInfo{};
		blendInfo.setAttachments(blendAttachment);
		const std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicInfo{};
		dynamicInfo.setDynamicStates(dynamicStates);
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		const std::array<vk::PushConstantRange, 1> pushConstantRanges = {
			vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eFragment)
				.setOffset(0).setSize(sizeof(std::array<u32, 4>))
		};
		pipelineLayoutInfo.setSetLayouts(DescriptorSetLayout).setPushConstantRanges(pushConstantRanges);
		PipelineLayout = Device.createPipelineLayout(pipelineLayoutInfo);
		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.setStages(stages)
			.setPVertexInputState(&vertexInputInfo)
			.setPInputAssemblyState(&inputAssemblyInfo)
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterizationInfo)
			.setPMultisampleState(&multisampleInfo)
			.setPColorBlendState(&blendInfo)
			.setPDynamicState(&dynamicInfo)
			.setLayout(PipelineLayout)
			.setRenderPass(ResolveRenderPass)
			.setSubpass(0);
		ResolvePipeline = Device.createGraphicsPipeline(nullptr, pipelineInfo).value;
		stages[1].setModule(PresentFragmentModule);
		pipelineInfo.setStages(stages).setRenderPass(RenderPass);
		Pipeline = Device.createGraphicsPipeline(nullptr, pipelineInfo).value;
		return true;
	}
	catch (...)
	{
		Finalize();
		return false;
	}
}
void Game::Filter::SetCopyFilter(const std::array<u8, 7>& coefficients)
{
	for (std::size_t i = 0; i < CopyFilterTaps.size(); ++i)
	{
		CopyFilterTaps[i] = std::min<u8>(coefficients[i], 63);
	}
}
void Game::Filter::Record(const gx::NativeFrameContext& frame)
{
	if (!Device || !ResolvePipeline || !Pipeline || frame.ScenePassActive || !frame.CommandBuffer || frame.ImageIndex >= ResolveFramebuffers.size() || frame.ImageIndex >= Framebuffers.size() || frame.ImageIndex >= ResolveDescriptorSets.size() || frame.ImageIndex >= DescriptorSets.size())
	{
		return;
	}
	vk::RenderPassBeginInfo resolveBeginInfo{};
	resolveBeginInfo.setRenderPass(ResolveRenderPass)
		.setFramebuffer(ResolveFramebuffers[frame.ImageIndex])
		.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(VideoPairWidth, VideoFrameHeight)));
	frame.CommandBuffer.beginRenderPass(resolveBeginInfo, vk::SubpassContents::eInline);
	const vk::Viewport videoFrameViewport = vk::Viewport()
		.setX(0.0f).setY(0.0f)
		.setWidth(static_cast<f32>(VideoPairWidth))
		.setHeight(static_cast<f32>(VideoFrameHeight))
		.setMinDepth(0.0f).setMaxDepth(1.0f);
	frame.CommandBuffer.setViewport(0, 1, &videoFrameViewport);
	const vk::Rect2D videoFrameScissor(vk::Offset2D(0, 0), vk::Extent2D(VideoPairWidth, VideoFrameHeight));
	frame.CommandBuffer.setScissor(0, 1, &videoFrameScissor);
	frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, ResolvePipeline);
	frame.CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PipelineLayout, 0, ResolveDescriptorSets[frame.ImageIndex], {});
	const std::array<u32, 4> rowWeights = {
		static_cast<u32>(CopyFilterTaps[0]) + CopyFilterTaps[1],
		static_cast<u32>(CopyFilterTaps[2]) + CopyFilterTaps[3] + CopyFilterTaps[4],
		static_cast<u32>(CopyFilterTaps[5]) + CopyFilterTaps[6],
		0u
	};
	frame.CommandBuffer.pushConstants(PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(rowWeights), rowWeights.data());
	frame.CommandBuffer.draw(3, 1, 0, 0);
	frame.CommandBuffer.endRenderPass();
	vk::RenderPassBeginInfo presentBeginInfo{};
	presentBeginInfo.setRenderPass(RenderPass)
		.setFramebuffer(Framebuffers[frame.ImageIndex])
		.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), frame.Extent));
	frame.CommandBuffer.beginRenderPass(presentBeginInfo, vk::SubpassContents::eInline);
	const vk::Viewport viewport = vk::Viewport()
		.setX(0.0f).setY(0.0f)
		.setWidth(static_cast<f32>(frame.Extent.width))
		.setHeight(static_cast<f32>(frame.Extent.height))
		.setMinDepth(0.0f).setMaxDepth(1.0f);
	frame.CommandBuffer.setViewport(0, 1, &viewport);
	const vk::Rect2D scissor(vk::Offset2D(0, 0), frame.Extent);
	frame.CommandBuffer.setScissor(0, 1, &scissor);
	frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);
	frame.CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PipelineLayout, 0, DescriptorSets[frame.ImageIndex], {});
	const bool outputIsSrgb = frame.ColorFormat == vk::Format::eB8G8R8A8Srgb || frame.ColorFormat == vk::Format::eR8G8B8A8Srgb;
	const std::array<u32, 4> presentSettings = { 0u, 0u, 0u, outputIsSrgb ? 1u : 0u };
	frame.CommandBuffer.pushConstants(PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(presentSettings), presentSettings.data());
	frame.CommandBuffer.draw(3, 1, 0, 0);
	frame.CommandBuffer.endRenderPass();
}
void Game::Filter::Finalize()
{
	if (!Device)
	{
		return;
	}
	Device.waitIdle();
	if (ResolvePipeline) Device.destroyPipeline(ResolvePipeline);
	if (Pipeline) Device.destroyPipeline(Pipeline);
	if (PipelineLayout) Device.destroyPipelineLayout(PipelineLayout);
	if (DescriptorPool) Device.destroyDescriptorPool(DescriptorPool);
	ResolveDescriptorSets.clear();
	DescriptorSets.clear();
	if (Sampler) Device.destroySampler(Sampler);
	if (DescriptorSetLayout) Device.destroyDescriptorSetLayout(DescriptorSetLayout);
	if (VertexModule) Device.destroyShaderModule(VertexModule);
	if (ResolveFragmentModule) Device.destroyShaderModule(ResolveFragmentModule);
	if (PresentFragmentModule) Device.destroyShaderModule(PresentFragmentModule);
	for (vk::Framebuffer framebuffer : ResolveFramebuffers)
	{
		if (framebuffer) Device.destroyFramebuffer(framebuffer);
	}
	ResolveFramebuffers.clear();
	for (vk::Framebuffer framebuffer : Framebuffers)
	{
		if (framebuffer) Device.destroyFramebuffer(framebuffer);
	}
	Framebuffers.clear();
	if (ResolveRenderPass) Device.destroyRenderPass(ResolveRenderPass);
	if (RenderPass) Device.destroyRenderPass(RenderPass);
	for (vk::ImageView imageView : VideoFrameImageViews)
	{
		if (imageView) Device.destroyImageView(imageView);
	}
	VideoFrameImageViews.clear();
	for (vk::Image image : VideoFrameImages)
	{
		if (image) Device.destroyImage(image);
	}
	VideoFrameImages.clear();
	for (vk::DeviceMemory memory : VideoFrameMemories)
	{
		if (memory) Device.freeMemory(memory);
	}
	VideoFrameMemories.clear();
	ResolvePipeline = nullptr;
	Pipeline = nullptr;
	PipelineLayout = nullptr;
	DescriptorPool = nullptr;
	Sampler = nullptr;
	DescriptorSetLayout = nullptr;
	VertexModule = nullptr;
	ResolveFragmentModule = nullptr;
	PresentFragmentModule = nullptr;
	ResolveRenderPass = nullptr;
	RenderPass = nullptr;
	Device = nullptr;
}