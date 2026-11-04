#include<Texture.h>
#include<Display.h>
#include<Shader.h>
#include"Data/TextureData.h"
#include"Data/ShaderData.h"
#include"Data/DisplayData.h"
#include"GxUtil.h"
#include<Array.h>
#include<cstring>
#include<limits>
gx::Texture::Texture() : Data(nullptr)
{

}
gx::Texture::~Texture()
{
	Finalize();
}
bool gx::Texture::Initialize(Display& display, Shader& shader, u32 width, u32 height, const u8* rgbaPixels)
{
	Finalize();
	if (!rgbaPixels || !InitializeImpl(display, shader, width, height, 1, TextureFilter::Linear, TextureFormat::Rgba8Unorm, false))
	{
		return false;
	}
	const u64 imageBytes = static_cast<u64>(width) * height * 4;
	if (!UploadLayer(rgbaPixels, imageBytes, 0))
	{
		Finalize();
		return false;
	}
	return IsValid();
}
bool gx::Texture::InitializeArray(Display& display, Shader& shader, u32 width, u32 height, u32 layers, TextureFilter filter, TextureFormat format)
{
	Finalize();
	return InitializeImpl(display, shader, width, height, layers, filter, format, true);
}
bool gx::Texture::InitializeImpl(Display& display, Shader& shader, u32 width, u32 height, u32 layers, TextureFilter filter, TextureFormat format, bool isArray)
{
	if (width == 0 || height == 0 || layers == 0 ||
		static_cast<u64>(width) * height > (std::numeric_limits<u64>::max)() / 4 ||
		!shader.Data || !shader.Data->DescriptorSetLayout || !shader.Data->TransformBuffer)
	{
		return false;
	}
	const internal::Device* deviceInfo = display.GetDeviceInfo();
	if (!deviceInfo || !deviceInfo->DeviceDat.Device || !deviceInfo->PhyDeviceDat.PhyDevice)
	{
		return false;
	}
	Data = Mem::MakeUnique<TextureData>();
	if (!Data)
	{
		return false;
	}
	Data->Device = deviceInfo->DeviceDat.Device;
	Data->PhysicalDevice = deviceInfo->PhyDeviceDat.PhyDevice;
	Data->Queue = deviceInfo->DeviceDat.Queue;
	Data->QueueFamily = deviceInfo->DeviceDat.GfxFamily;
	Data->Width = width;
	Data->Height = height;
	Data->Layers = layers;
	Data->Format = format;
	Data->VkFormat = internal::GetVkFormat(format);
	Data->IsArray = isArray;
	if (Data->VkFormat == vk::Format::eUndefined)
	{
		Finalize();
		return false;
	}
	try
	{
		Data->UploadedLayers.assign(layers, false);
		vk::ImageCreateInfo imageInfo{};
		imageInfo.setImageType(vk::ImageType::e2D)
			.setExtent(vk::Extent3D(width, height, 1))
			.setMipLevels(1)
			.setArrayLayers(layers)
			.setFormat(Data->VkFormat)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1);
		Data->Image = Data->Device.createImage(imageInfo);
		const vk::MemoryRequirements imageRequirements = Data->Device.getImageMemoryRequirements(Data->Image);
		const u32 imageMemoryType = internal::FindMemoryType(deviceInfo->PhyDeviceDat.PhyDevice, imageRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		if (imageMemoryType == UINT32_MAX)
		{
			Finalize();
			return false;
		}
		vk::MemoryAllocateInfo imageAllocation{};
		imageAllocation.setAllocationSize(imageRequirements.size)
			.setMemoryTypeIndex(imageMemoryType);
		Data->ImageMemory = Data->Device.allocateMemory(imageAllocation);
		Data->Device.bindImageMemory(Data->Image, Data->ImageMemory, 0);

		const vk::ImageSubresourceRange imageRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, layers);
		vk::ImageViewCreateInfo imageViewInfo{};
		imageViewInfo.setImage(Data->Image)
			.setViewType(isArray ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D)
			.setFormat(Data->VkFormat)
			.setComponents(internal::GetComponentMapping(format))
			.setSubresourceRange(imageRange);
		Data->ImageView = Data->Device.createImageView(imageViewInfo);

		const vk::Filter vkFilter = filter == TextureFilter::Nearest ? vk::Filter::eNearest : vk::Filter::eLinear;
		vk::SamplerCreateInfo samplerInfo{};
		samplerInfo.setMagFilter(vkFilter)
			.setMinFilter(vkFilter)
			.setMipmapMode(filter == TextureFilter::Nearest ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear)
			.setAddressModeU(isArray ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(isArray ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
			.setMaxAnisotropy(1.0f)
			.setMinLod(0.0f)
			.setMaxLod(0.0f);
		Data->Sampler = Data->Device.createSampler(samplerInfo);

		Array<vk::DescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1);
		poolSizes[1].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1);
		vk::DescriptorPoolCreateInfo poolInfo{};
		poolInfo.setMaxSets(1).setPoolSizes(poolSizes);
		Data->DescriptorPool = Data->Device.createDescriptorPool(poolInfo);
		vk::DescriptorSetAllocateInfo setAllocation{};
		setAllocation.setDescriptorPool(Data->DescriptorPool)
			.setSetLayouts(shader.Data->DescriptorSetLayout);
		Data->DescriptorSet = Data->Device.allocateDescriptorSets(setAllocation).front();
		vk::DescriptorImageInfo descriptorImage{};
		descriptorImage.setSampler(Data->Sampler)
			.setImageView(Data->ImageView)
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::WriteDescriptorSet descriptorWrite{};
		descriptorWrite.setDstSet(Data->DescriptorSet)
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(descriptorImage);
		vk::DescriptorBufferInfo transformBufferInfo{};
		transformBufferInfo.setBuffer(shader.Data->TransformBuffer)
			.setOffset(0)
			.setRange(sizeof(f32) * 48);
		vk::WriteDescriptorSet transformWrite{};
		transformWrite.setDstSet(Data->DescriptorSet)
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(transformBufferInfo);
		Array<vk::WriteDescriptorSet, 2> descriptorWrites = { descriptorWrite, transformWrite };
		Data->Device.updateDescriptorSets(descriptorWrites, {});
	}
	catch (...)
	{
		Finalize();
		return false;
	}
	return Data && Data->Image && Data->ImageView && Data->Sampler && Data->DescriptorSet;
}
bool gx::Texture::UploadLayer(const void* pixelData, u64 sizeInBytes, u32 layer)
{
	if (!Data || !Data->Device || !Data->Image || !pixelData || layer >= Data->Layers)
	{
		return false;
	}
	const u64 pixelCount = internal::GetPixelCount(Data->Width, Data->Height);
	const u64 sourceSize = internal::GetSourceLayerSize(Data->Format, Data->VkFormat, Data->Width, Data->Height);
	const u64 uploadSize = internal::GetUploadLayerSize(Data->Format, Data->VkFormat, Data->Width, Data->Height);
	if (sourceSize == 0 || uploadSize == 0 || uploadSize > (std::numeric_limits<size_t>::max)())
	{
		return false;
	}
	DynamicArray<u8> convertedData;
	const void* uploadData = pixelData;
	if ((Data->Format == TextureFormat::A4 || Data->Format == TextureFormat::L4) && sizeInBytes == sourceSize)
	{
		convertedData.resize(static_cast<size_t>(uploadSize));
		internal::Expand4BitSingleChannelData(static_cast<const u8*>(pixelData), convertedData.data(), pixelCount);
		uploadData = convertedData.data();
	}
	else if (Data->Format == TextureFormat::L4A4 && sizeInBytes == sourceSize)
	{
		convertedData.resize(static_cast<size_t>(uploadSize));
		internal::ExpandLA4Data(static_cast<const u8*>(pixelData), convertedData.data(), pixelCount);
		uploadData = convertedData.data();
	}
	else if (sizeInBytes != uploadSize)
	{
		return false;
	}
	vk::Buffer stagingBuffer{};
	vk::DeviceMemory stagingMemory{};
	vk::CommandPool commandPool{};
	try
	{
		vk::BufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.setSize(uploadSize)
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
			.setSharingMode(vk::SharingMode::eExclusive);
		stagingBuffer = Data->Device.createBuffer(stagingBufferInfo);
		const vk::MemoryRequirements stagingRequirements = Data->Device.getBufferMemoryRequirements(stagingBuffer);
		const u32 stagingMemoryType = internal::FindMemoryType(Data->PhysicalDevice, stagingRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		if (stagingMemoryType == UINT32_MAX)
		{
			Data->Device.destroyBuffer(stagingBuffer);
			return false;
		}
		vk::MemoryAllocateInfo stagingAllocation{};
		stagingAllocation.setAllocationSize(stagingRequirements.size)
			.setMemoryTypeIndex(stagingMemoryType);
		stagingMemory = Data->Device.allocateMemory(stagingAllocation);
		Data->Device.bindBufferMemory(stagingBuffer, stagingMemory, 0);
		void* mappedPixels = Data->Device.mapMemory(stagingMemory, 0, uploadSize);
		if (!mappedPixels)
		{
			Data->Device.destroyBuffer(stagingBuffer);
			Data->Device.freeMemory(stagingMemory);
			return false;
		}
		std::memcpy(mappedPixels, uploadData, static_cast<size_t>(uploadSize));
		Data->Device.unmapMemory(stagingMemory);

		vk::CommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.setQueueFamilyIndex(Data->QueueFamily)
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient);
		commandPool = Data->Device.createCommandPool(commandPoolInfo);
		vk::CommandBufferAllocateInfo commandAllocation{};
		commandAllocation.setCommandPool(commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		vk::CommandBuffer commandBuffer = Data->Device.allocateCommandBuffers(commandAllocation).front();
		commandBuffer.begin(vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		const vk::ImageSubresourceRange imageRange(vk::ImageAspectFlagBits::eColor, 0, 1, layer, 1);
		const bool wasUploaded = Data->UploadedLayers[layer];
		vk::ImageMemoryBarrier toTransfer{};
		toTransfer.setOldLayout(wasUploaded ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined)
			.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(Data->Image)
			.setSubresourceRange(imageRange)
			.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		if (wasUploaded)
		{
			toTransfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
		}
		commandBuffer.pipelineBarrier(
			wasUploaded ? vk::PipelineStageFlagBits::eFragmentShader : vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, toTransfer);
		vk::BufferImageCopy copyRegion{};
		copyRegion.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, layer, 1))
			.setImageExtent(vk::Extent3D(Data->Width, Data->Height, 1));
		commandBuffer.copyBufferToImage(stagingBuffer, Data->Image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
		vk::ImageMemoryBarrier toShader{};
		toShader.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
			.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(Data->Image)
			.setSubresourceRange(imageRange)
			.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
			{}, {}, {}, toShader);
		commandBuffer.end();
		Data->Device.waitIdle();
		vk::SubmitInfo submitInfo{};
		submitInfo.setCommandBuffers(commandBuffer);
		Data->Queue.submit(submitInfo, nullptr);
		Data->Queue.waitIdle();
		Data->Device.destroyCommandPool(commandPool);
		commandPool = nullptr;
		Data->Device.destroyBuffer(stagingBuffer);
		stagingBuffer = nullptr;
		Data->Device.freeMemory(stagingMemory);
		stagingMemory = nullptr;
		Data->UploadedLayers[layer] = true;
		return true;
	}
	catch (...)
	{
		if (commandPool) Data->Device.destroyCommandPool(commandPool);
		if (stagingBuffer) Data->Device.destroyBuffer(stagingBuffer);
		if (stagingMemory) Data->Device.freeMemory(stagingMemory);
		return false;
	}
}
void gx::Texture::Finalize()
{
	if (!Data)
	{
		return;
	}
	if (Data->Device)
	{
		Data->Device.waitIdle();
		if (Data->DescriptorPool)
		{
			Data->Device.destroyDescriptorPool(Data->DescriptorPool);
		}
		if (Data->Sampler)
		{
			Data->Device.destroySampler(Data->Sampler);
		}
		if (Data->ImageView)
		{
			Data->Device.destroyImageView(Data->ImageView);
		}
		if (Data->Image)
		{
			Data->Device.destroyImage(Data->Image);
		}
		if (Data->ImageMemory)
		{
			Data->Device.freeMemory(Data->ImageMemory);
		}
	}
	Data.reset();
}
bool gx::Texture::IsValid() const
{
	return Data && Data->Device && Data->Image && Data->ImageView && Data->Sampler && Data->DescriptorSet;
}
