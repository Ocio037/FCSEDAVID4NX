#include<CommandBuffer.h>
#include<Array.h>
#include"Data/CommandBufferData.h"
#include<Display.h>
#include<Shader.h>
#include"Data/ShaderData.h"
#include<Buffer.h>
#include<Texture.h>
#include"Data/BufferData.h"
#include"Data/TextureData.h"
gx::CommandBuffer::CommandBuffer() : Data(nullptr)
{

}
gx::CommandBuffer::CommandBuffer(Display& display) : Data(nullptr)
{
    Initialize(display);
}
gx::CommandBuffer::~CommandBuffer()
{
    Finalize();
}
void gx::CommandBuffer::Initialize(gx::Display& display)
{
    Finalize();
    Data = Mem::MakeUnique<CommandBufferData>();
    if (!Data) return;
    const internal::Device* Device = display.GetDeviceInfo();
    if (!Device || !Device->DeviceDat.Device || Device->DeviceDat.DepthFormat == vk::Format::eUndefined)
    {
        Data.reset();
        return;
    }
    Data->Device = Device->DeviceDat.Device;
    Data->PhysicalDevice = Device->PhyDeviceDat.PhyDevice;
    Data->Queue = Device->DeviceDat.Queue;
    Data->Swapchain = Device->DeviceDat.Swapchain;
    Data->ImageFormat = Device->DeviceDat.ImageFormat;
    Data->SceneFormat = Device->DeviceDat.SceneFormat;
    Data->DepthFormat = Device->DeviceDat.DepthFormat;
    Data->SwapchainImageViews = Device->DeviceDat.ImagesViews;
    Data->PresentWaitEnabled = Device->DeviceDat.PresentWaitEnabled;
    if (Data->PresentWaitEnabled)
    {
        Data->WaitForPresent = reinterpret_cast<PFN_vkWaitForPresentKHR>(vkGetDeviceProcAddr(static_cast<VkDevice>(Data->Device), "vkWaitForPresentKHR"));
        Data->PresentWaitEnabled = Data->WaitForPresent != nullptr;
    }
    Data->Extent = vk::Extent2D(Device->PhyDeviceDat.Resolution.Width, Device->PhyDeviceDat.Resolution.Height);
    Data->SceneExtent = Data->Extent;
    Data->Clear[0].setColor(vk::ClearColorValue(Array<float, 4>{0.1f, 0.2f, 0.3f, 1.0f}));
    Data->Clear[1].setDepthStencil(vk::ClearDepthStencilValue(1.0f, 0));
    Data->Clear[2].setDepthStencil(vk::ClearDepthStencilValue(1.0f, 0));
    vk::ImageCreateInfo depthImageInfo{};
    depthImageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(Device->DeviceDat.DepthFormat)
        .setExtent(vk::Extent3D(Data->Extent.width, Data->Extent.height, 1))
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    Data->DepthImage = Data->Device.createImage(depthImageInfo);
    const auto depthMemoryRequirements = Data->Device.getImageMemoryRequirements(Data->DepthImage);
    const u32 depthMemoryType = internal::FindMemoryType(Device->PhyDeviceDat.PhyDevice, depthMemoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (depthMemoryType == UINT32_MAX)
    {
        Finalize();
        return;
    }
    vk::MemoryAllocateInfo depthAllocation{};
    depthAllocation.setAllocationSize(depthMemoryRequirements.size)
        .setMemoryTypeIndex(depthMemoryType);
    Data->DepthMemory = Data->Device.allocateMemory(depthAllocation);
    Data->Device.bindImageMemory(Data->DepthImage, Data->DepthMemory, 0);
    vk::ImageViewCreateInfo depthViewInfo{};
    depthViewInfo.setImage(Data->DepthImage)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(Device->DeviceDat.DepthFormat)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
    Data->DepthImageView = Data->Device.createImageView(depthViewInfo);
    vk::AttachmentDescription ColorAttachment = vk::AttachmentDescription()
        .setFormat(Device->DeviceDat.ImageFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
    vk::AttachmentDescription DepthAttachment = vk::AttachmentDescription()
        .setFormat(Device->DeviceDat.DepthFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    vk::AttachmentReference ColorRef = vk::AttachmentReference()
        .setAttachment(0)
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference DepthRef = vk::AttachmentReference()
        .setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    vk::SubpassDescription SubPass = vk::SubpassDescription()
        .setColorAttachmentCount(1)
        .setColorAttachments(ColorRef)
        .setPDepthStencilAttachment(&DepthRef)
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    const Array<vk::AttachmentDescription, 2> attachments = { ColorAttachment, DepthAttachment };
    Data->RenderPassAttachmentCount = static_cast<u32>(attachments.size());
    const vk::SubpassDependency dependency = vk::SubpassDependency()
        .setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
            vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
            vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    vk::CommandPoolCreateInfo PoolInfo = vk::CommandPoolCreateInfo()
        .setQueueFamilyIndex(Device->DeviceDat.GfxFamily)
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    Data->CommandPool = Data->Device.createCommandPool(PoolInfo);
    vk::CommandBufferAllocateInfo AllocInfo = vk::CommandBufferAllocateInfo()
        .setCommandPool(Data->CommandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
    Data->CommandBuffer = Data->Device.allocateCommandBuffers(AllocInfo).front();
    vk::RenderPassCreateInfo RenderPassInfo = vk::RenderPassCreateInfo()
        .setAttachments(attachments)
        .setSubpasses(SubPass)
        .setDependencies(dependency);
    Data->RenderPass = Data->Device.createRenderPass(RenderPassInfo);
    Data->Framebuffers.reserve(Device->DeviceDat.ImagesViews.size());
    for (std::size_t i = 0; i < Device->DeviceDat.ImagesViews.size(); ++i)
    {
        const Array<vk::ImageView, 2> framebufferAttachments = {
            Device->DeviceDat.ImagesViews[i], Data->DepthImageView };
        vk::FramebufferCreateInfo Info = vk::FramebufferCreateInfo()
            .setRenderPass(Data->RenderPass)
            .setAttachments(framebufferAttachments)
            .setWidth(Data->Extent.width)
            .setHeight(Data->Extent.height)
            .setLayers(1);
        Data->Framebuffers.push_back(Data->Device.createFramebuffer(Info));
    }
    vk::SemaphoreCreateInfo SemaphoreInfo{};
    Data->ImageAvailableSemaphore = Data->Device.createSemaphore(SemaphoreInfo);
    Data->RenderFinishedSemaphore = Data->Device.createSemaphore(SemaphoreInfo);
    vk::FenceCreateInfo FenceInfo = vk::FenceCreateInfo()
        .setFlags(vk::FenceCreateFlagBits::eSignaled);
    Data->InFlightFence = Data->Device.createFence(FenceInfo);
}
bool gx::CommandBuffer::InitializeOffscreenScene(u32 width, u32 height, bool useSceneFormat)
{
    if (!Data || Data->OffscreenSceneEnabled || width == 0 || height == 0 || width > Data->Extent.width || height > Data->Extent.height)
    {
        return false;
    }
    try
    {
        Data->Device.waitIdle();
        Data->SceneExtent = vk::Extent2D(width, height);
        const vk::Format sceneFormat = useSceneFormat ? Data->SceneFormat : Data->ImageFormat;
        const std::size_t imageCount = Data->Framebuffers.size();
        Data->SceneImages.resize(imageCount);
        Data->SceneMemories.resize(imageCount);
        Data->SceneImageViews.resize(imageCount);
        for (std::size_t i = 0; i < imageCount; ++i)
        {
            vk::ImageCreateInfo imageInfo{};
            imageInfo.setImageType(vk::ImageType::e2D)
                .setFormat(sceneFormat)
                .setExtent(vk::Extent3D(Data->SceneExtent.width, Data->SceneExtent.height, 1))
                .setMipLevels(1)
                .setArrayLayers(1)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setTiling(vk::ImageTiling::eOptimal)
                .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setInitialLayout(vk::ImageLayout::eUndefined);
            Data->SceneImages[i] = Data->Device.createImage(imageInfo);
            const auto memoryRequirements = Data->Device.getImageMemoryRequirements(Data->SceneImages[i]);
            const u32 memoryType = internal::FindMemoryType(Data->PhysicalDevice, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
            if (memoryType == UINT32_MAX)
            {
                Finalize();
                return false;
            }
            vk::MemoryAllocateInfo allocation{};
            allocation.setAllocationSize(memoryRequirements.size).setMemoryTypeIndex(memoryType);
            Data->SceneMemories[i] = Data->Device.allocateMemory(allocation);
            Data->Device.bindImageMemory(Data->SceneImages[i], Data->SceneMemories[i], 0);
            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.setImage(Data->SceneImages[i])
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(sceneFormat)
                .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
            Data->SceneImageViews[i] = Data->Device.createImageView(viewInfo);
        }
        for (vk::Framebuffer framebuffer : Data->Framebuffers)
        {
            Data->Device.destroyFramebuffer(framebuffer);
        }
        Data->Framebuffers.clear();
        Data->Device.destroyRenderPass(Data->RenderPass);
        Data->RenderPass = nullptr;
        vk::AttachmentDescription colorAttachment{};
        colorAttachment.setFormat(sceneFormat)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::AttachmentDescription depthAttachment{};
        depthAttachment.setFormat(Data->DepthFormat)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        const vk::AttachmentReference colorRef = vk::AttachmentReference()
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);
        const vk::AttachmentReference depthRef = vk::AttachmentReference()
            .setAttachment(1)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        vk::SubpassDescription subpass{};
        subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachmentCount(1)
            .setColorAttachments(colorRef)
            .setPDepthStencilAttachment(&depthRef);
        const Array<vk::AttachmentDescription, 2> sceneAttachments = { colorAttachment, depthAttachment };
        const vk::SubpassDependency sceneDependency = vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        vk::RenderPassCreateInfo scenePassInfo{};
        scenePassInfo.setAttachments(sceneAttachments).setSubpasses(subpass).setDependencies(sceneDependency);
        Data->RenderPass = Data->Device.createRenderPass(scenePassInfo);
        Data->RenderPassAttachmentCount = static_cast<u32>(sceneAttachments.size());
        Data->Framebuffers.reserve(imageCount);
        for (std::size_t i = 0; i < imageCount; ++i)
        {
            const Array<vk::ImageView, 2> attachments = { Data->SceneImageViews[i], Data->DepthImageView };
            vk::FramebufferCreateInfo framebufferInfo{};
            framebufferInfo.setRenderPass(Data->RenderPass)
                .setAttachments(attachments)
                .setWidth(Data->SceneExtent.width)
                .setHeight(Data->SceneExtent.height)
                .setLayers(1);
            Data->Framebuffers.push_back(Data->Device.createFramebuffer(framebufferInfo));
        }
        Data->OffscreenSceneEnabled = true;
        return true;
    }
    catch (...)
    {
        Finalize();
        return false;
    }
}
void gx::CommandBuffer::Finalize()
{
    if (!Data) return;
    if (Data->Device)
    {
        Data->Device.waitIdle();
    }
    if (Data->Device)
    {
        if (Data->ImageAvailableSemaphore)
        {
            Data->Device.destroySemaphore(Data->ImageAvailableSemaphore);
        }
        if (Data->RenderFinishedSemaphore)
        {
            Data->Device.destroySemaphore(Data->RenderFinishedSemaphore);
        }
        if (Data->InFlightFence)
        {
            Data->Device.destroyFence(Data->InFlightFence);
        }
        for (vk::Framebuffer Framebuffer : Data->Framebuffers)
        {
            if (Framebuffer)
            {
                Data->Device.destroyFramebuffer(Framebuffer);
            }
        }
        Data->Framebuffers.clear();
        for (vk::ImageView imageView : Data->SceneImageViews)
        {
            if (imageView)
            {
                Data->Device.destroyImageView(imageView);
            }
        }
        Data->SceneImageViews.clear();
        for (vk::Image image : Data->SceneImages)
        {
            if (image)
            {
                Data->Device.destroyImage(image);
            }
        }
        Data->SceneImages.clear();
        for (vk::DeviceMemory memory : Data->SceneMemories)
        {
            if (memory)
            {
                Data->Device.freeMemory(memory);
            }
        }
        Data->SceneMemories.clear();
        if (Data->DepthImageView)
        {
            Data->Device.destroyImageView(Data->DepthImageView);
            Data->DepthImageView = nullptr;
        }
        if (Data->DepthImage)
        {
            Data->Device.destroyImage(Data->DepthImage);
            Data->DepthImage = nullptr;
        }
        if (Data->DepthMemory)
        {
            Data->Device.freeMemory(Data->DepthMemory);
            Data->DepthMemory = nullptr;
        }
        if (Data->RenderPass)
        {
            Data->Device.destroyRenderPass(Data->RenderPass);
        }
        if (Data->CommandPool)
        {
            Data->Device.destroyCommandPool(Data->CommandPool);
        }
    }
    Data.reset();
}
void gx::CommandBuffer::Render()
{
    if (!Data)
    {
        return;
    }
    // Acquire after the game loop ends idle so image and GPU waits belong to GP.
    if (!Data->ImageAcquired)
    {
        Data->Device.waitForFences(Data->InFlightFence, VK_TRUE, UINT64_MAX);
        Data->ImageIndex = Data->Device.acquireNextImageKHR(
            Data->Swapchain, UINT64_MAX, Data->ImageAvailableSemaphore, nullptr).value;
        Data->Device.resetFences(Data->InFlightFence);
    }
    Data->ImageAcquired = false;
    Data->Submitted = false;
    Data->CommandBuffer.reset();
    Data->CurrentPipeline = nullptr;
    Data->CurrentShader = nullptr;
    Data->TransparencyPass = false;
    Data->OverlayPass = false;
    Data->CommandBuffer.begin(vk::CommandBufferBeginInfo{});
    vk::RenderPassBeginInfo BeginInfo = vk::RenderPassBeginInfo()
        .setRenderPass(Data->RenderPass)
        .setFramebuffer(Data->Framebuffers[Data->ImageIndex])
        .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), Data->SceneExtent))
        .setClearValueCount(Data->RenderPassAttachmentCount)
        .setPClearValues(Data->Clear.data());
    Data->CommandBuffer.beginRenderPass(BeginInfo, vk::SubpassContents::eInline);
    Data->ScenePassActive = true;
    vk::Viewport viewport = vk::Viewport()
        .setX(0.0f)
        .setY(0.0f)
        .setWidth((float)Data->SceneExtent.width)
        .setHeight((float)Data->SceneExtent.height)
        .setMinDepth(0.0f)
        .setMaxDepth(1.0f);
    Data->CommandBuffer.setViewport(0, 1, &viewport);
    vk::Rect2D scissor = vk::Rect2D()
        .setOffset(vk::Offset2D(0, 0))
        .setExtent(Data->SceneExtent);
    Data->CommandBuffer.setScissor(0, 1, &scissor);
}
void gx::CommandBuffer::WaitForPresent()
{
	if (!Data)
	{
		return;
	}
	bool waitedForPresent = false;
	if (Data->PresentWaitEnabled && Data->PendingPresentId != 0)
	{
		const VkResult result = Data->WaitForPresent(
			static_cast<VkDevice>(Data->Device),
			static_cast<VkSwapchainKHR>(Data->Swapchain),
			Data->PendingPresentId,
			UINT64_MAX);
		if (result != VK_SUCCESS)
		{
			Data->PresentWaitEnabled = false;
		}
		else
		{
			waitedForPresent = true;
		}
		Data->PendingPresentId = 0;
	}
	if (!waitedForPresent && Data->PresentationPending)
	{
        internal::WaitForDwmPresentIfAvailable();
	}
	Data->PresentationPending = false;
}
void gx::CommandBuffer::EndScenePass()
{
    if (Data && Data->ScenePassActive)
    {
        Data->CommandBuffer.endRenderPass();
        Data->ScenePassActive = false;
    }
}
gx::NativeFrameContext gx::CommandBuffer::GetNativeFrameContext() const
{
    NativeFrameContext context{};
    if (!Data)
    {
        return context;
    }
    context.Device = Data->Device;
    context.PhysicalDevice = Data->PhysicalDevice;
    context.CommandBuffer = Data->CommandBuffer;
    context.Extent = Data->Extent;
    context.SceneExtent = Data->SceneExtent;
    context.SceneSampleCount = 1;
    context.ColorFormat = Data->ImageFormat;
    context.ImageIndex = Data->ImageIndex;
    context.ScenePassActive = Data->ScenePassActive;
    context.SceneImageViews = &Data->SceneImageViews;
    context.OutputImageViews = &Data->SwapchainImageViews;
    return context;
}
void gx::CommandBuffer::Submit()
{
    if (!Data || Data->Submitted)
    {
        return;
    }
    EndScenePass();
    Data->CommandBuffer.end();
    const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    const vk::SubmitInfo submitInfo = vk::SubmitInfo()
        .setWaitSemaphores(Data->ImageAvailableSemaphore)
        .setWaitDstStageMask(waitStage)
        .setCommandBuffers(Data->CommandBuffer)
        .setSignalSemaphores(Data->RenderFinishedSemaphore);
    Data->Queue.submit(submitInfo, Data->InFlightFence);
    Data->Submitted = true;
}
void gx::CommandBuffer::WaitForGpu()
{
	if (Data && Data->Submitted)
	{
		Data->Device.waitForFences(Data->InFlightFence, VK_TRUE, UINT64_MAX);
	}
}
void gx::CommandBuffer::Present()
{
	if (!Data)
	{
		return;
	}
	if (!Data->Submitted)
	{
		Submit();
	}
    vk::PresentInfoKHR PresentInfo = vk::PresentInfoKHR()
        .setWaitSemaphores(Data->RenderFinishedSemaphore)
        .setSwapchains(Data->Swapchain)
        .setImageIndices(Data->ImageIndex);
    if (Data->PresentWaitEnabled)
    {
		const std::uint64_t presentId = Data->NextPresentId++;
		const vk::PresentIdKHR presentIdInfo = vk::PresentIdKHR()
			.setSwapchainCount(1)
			.setPPresentIds(&presentId);
		PresentInfo.setPNext(&presentIdInfo);
		Data->PendingPresentId = presentId;
	}
    Data->Queue.presentKHR(PresentInfo);
	Data->PresentationPending = true;
	Data->ImageAcquired = false;
	Data->Submitted = false;
}
void gx::CommandBuffer::SetColor(f32 r, f32 g, f32 b, f32 a)
{
    Data->Clear[0].setColor(vk::ClearColorValue(Array<float, 4>{r, g, b, a}));
}
void gx::CommandBuffer::SetShader(Shader* shader)
{
	if (!Data)
	{
		return;
	}
	if (!shader)
	{
		Data->PipelineLayout = nullptr;
		Data->CurrentPipeline = nullptr;
		Data->CurrentShader = nullptr;
		return;
	}
    if (shader && shader->Data)
    {
        Data->PipelineLayout = shader->Data->PipelineLayout;
        Data->CurrentShader = shader;
        Data->CurrentPipeline = Data->OverlayPass
			? shader->Data->OverlayPipeline
			: (Data->TransparencyPass ? shader->Data->TransparentPipeline : shader->Data->OpaquePipeline);
        if (Data->CurrentPipeline)
        {
            Data->CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, Data->CurrentPipeline);
        }
    }
}
void gx::CommandBuffer::PushFragmentConstants(const void* data, u32 sizeInBytes, u32 offsetInBytes)
{
	if (!Data || !Data->PipelineLayout || !data || sizeInBytes == 0)
	{
		return;
	}
	Data->CommandBuffer.pushConstants(Data->PipelineLayout, vk::ShaderStageFlagBits::eFragment,
		offsetInBytes, sizeInBytes, data);
}
void gx::CommandBuffer::SetTransparencyPass(bool enabled)
{
    Data->TransparencyPass = enabled;
    if (Data->CurrentShader)
    {
        SetShader(Data->CurrentShader);
    }
}
void gx::CommandBuffer::SetOverlayPass(bool enabled)
{
	Data->OverlayPass = enabled;
	if (Data->CurrentShader)
	{
		SetShader(Data->CurrentShader);
	}
}
void gx::CommandBuffer::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance)
{
    Data->CommandBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}
void gx::CommandBuffer::BindVertexBuffer(Buffer* buffer)
{
    if (buffer && buffer->Data && buffer->Data->HasVertex)
    {
        vk::DeviceSize offset = 0;
        Data->CommandBuffer.bindVertexBuffers(0, 1, &buffer->Data->VertexBuffer, &offset);
    }
}
void gx::CommandBuffer::BindIndexBuffer(Buffer* buffer)
{
    if (buffer && buffer->Data && buffer->Data->HasIndex)
    {
        Data->CommandBuffer.bindIndexBuffer(buffer->Data->IndexBuffer, 0, vk::IndexType::eUint32);
    }
}
void gx::CommandBuffer::BindTexture(Texture* texture)
{
    if (texture && texture->Data && texture->Data->DescriptorSet && Data->PipelineLayout)
    {
        Data->CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, Data->PipelineLayout, 0, texture->Data->DescriptorSet, {});
    }
}
void gx::CommandBuffer::DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, s32 vertexOffset, u32 firstInstance)
{
    Data->CommandBuffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}