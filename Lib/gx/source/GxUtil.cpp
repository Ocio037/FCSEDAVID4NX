#include "GxUtil.h"
u32 gx::internal::FindMemoryType(vk::PhysicalDevice physicalDevice, u32 typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
    for (u32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    return UINT32_MAX;
}
void gx::internal::WaitForDwmPresentIfAvailable()
{
	using DwmFlushFunction = HRESULT(WINAPI*)();
	static HMODULE DwmModule = LoadLibraryW(L"dwmapi.dll");
	static DwmFlushFunction DwmFlush = DwmModule ? reinterpret_cast<DwmFlushFunction>(GetProcAddress(DwmModule, "DwmFlush")) : nullptr;
	if (DwmFlush)
	{
		DwmFlush();
	}
}
vk::SampleCountFlags gx::internal::GetImageSampleCounts(vk::PhysicalDevice physicalDevice, vk::Format format, vk::ImageUsageFlags usage)
{
	try
	{
		return physicalDevice.getImageFormatProperties(format, vk::ImageType::e2D, vk::ImageTiling::eOptimal, usage, vk::ImageCreateFlags{}).sampleCounts;
	}
	catch (...)
	{
		return vk::SampleCountFlags{};
	}
}
bool gx::internal::HasDeviceExtension(vk::PhysicalDevice physicalDevice, const char* extensionName)
{
	for (const vk::ExtensionProperties& extension : physicalDevice.enumerateDeviceExtensionProperties())
	{
		if (std::strcmp(extension.extensionName, extensionName) == 0)
		{
			return true;
		}
	}
	return false;
}
bool gx::internal::HasPresentableGraphicsQueue(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
{
	const auto queueFamilies = physicalDevice.getQueueFamilyProperties();
	for (u32 index = 0; index < queueFamilies.size(); ++index)
	{
		if (queueFamilies[index].queueCount != 0 && (queueFamilies[index].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(index, surface))
		{
			return true;
		}
	}
	return false;
}
bool gx::internal::SupportsSingleSampleScene(vk::PhysicalDevice physicalDevice)
{
	const vk::Format sceneFormat = vk::Format::eR8G8B8A8Unorm;
	const vk::FormatFeatureFlags sceneFeatures = vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage;
	const vk::FormatProperties sceneProperties = physicalDevice.getFormatProperties(sceneFormat);
	const vk::ImageUsageFlags sceneUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
	if ((sceneProperties.optimalTilingFeatures & sceneFeatures) != sceneFeatures || !(GetImageSampleCounts(physicalDevice, sceneFormat, sceneUsage) & vk::SampleCountFlagBits::e1))
	{
		return false;
	}
	const Array<vk::Format, 4> depthFormats = {
		vk::Format::eD16Unorm,
		vk::Format::eD24UnormS8Uint,
		vk::Format::eD32Sfloat,
		vk::Format::eD32SfloatS8Uint,
	};
	for (vk::Format depthFormat : depthFormats)
	{
		const vk::FormatProperties depthProperties = physicalDevice.getFormatProperties(depthFormat);
		if ((depthProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) && (GetImageSampleCounts(physicalDevice, depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment) & vk::SampleCountFlagBits::e1))
		{
			return true;
		}
	}
	return false;
}
bool gx::internal::HasColorAttachmentSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
{
	const auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	const Array<vk::Format, 4> preferredFormats = {
		vk::Format::eB8G8R8A8Srgb,
		vk::Format::eR8G8B8A8Srgb,
		vk::Format::eB8G8R8A8Unorm,
		vk::Format::eR8G8B8A8Unorm,
	};
	if (surfaceFormats.size() == 1 && surfaceFormats.front().format == vk::Format::eUndefined)
	{
		for (vk::Format format : preferredFormats)
		{
			if (physicalDevice.getFormatProperties(format).optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
			{
				return true;
			}
		}
		return false;
	}
	for (const vk::SurfaceFormatKHR& surfaceFormat : surfaceFormats)
	{
		if (surfaceFormat.format != vk::Format::eUndefined && (physicalDevice.getFormatProperties(surfaceFormat.format).optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment))
		{
			return true;
		}
	}
	return false;
}
bool gx::internal::HasValidSpirvHeader(const void* shaderData, u32 shaderSize)
{
	static constexpr Array<u8, 4> magic = { 0x03, 0x02, 0x23, 0x07 };
	return shaderData && shaderSize >= magic.size() && shaderSize % sizeof(u32) == 0 && std::memcmp(shaderData, magic.data(), magic.size()) == 0;
}
vk::Format gx::internal::GetVkFormat(gx::TextureFormat format)
{
	switch (format)
	{
	case gx::TextureFormat::A4:
	case gx::TextureFormat::A8:
	case gx::TextureFormat::L4:
	case gx::TextureFormat::L8:
		return vk::Format::eR8Unorm;
	case gx::TextureFormat::L4A4:
	case gx::TextureFormat::L8A8:
		return vk::Format::eR8G8Unorm;
	case gx::TextureFormat::R5G6B5:
		return vk::Format::eR5G6B5UnormPack16;
	case gx::TextureFormat::R5G5B5A1:
		return vk::Format::eA1R5G5B5UnormPack16;
	case gx::TextureFormat::R4G4B4A4:
		return vk::Format::eR4G4B4A4UnormPack16;
	case gx::TextureFormat::R8G8B8A8:
		return vk::Format::eR8G8B8A8Unorm;
	case gx::TextureFormat::CTXE:
#ifdef _WIN32
		return vk::Format::eBc1RgbUnormBlock;
#else
		return vk::Format::eEtc2R8G8B8UnormBlock;
#endif
	case gx::TextureFormat::CTXE_A:
	case gx::TextureFormat::CTXE_A4:
#ifdef _WIN32
		return vk::Format::eBc2UnormBlock;
#else
		return vk::Format::eEtc2R8G8B8A8UnormBlock;
#endif
	default:
		return vk::Format::eUndefined;
	}
}
u64 gx::internal::GetPixelCount(u32 width, u32 height)
{
	return static_cast<u64>(width) * height;
}
u32 gx::internal::GetCompressedBlockSize(vk::Format format)
{
	switch (format)
	{
	case vk::Format::eBc1RgbUnormBlock:
	case vk::Format::eEtc2R8G8B8UnormBlock:
		return 8;
	case vk::Format::eBc2UnormBlock:
	case vk::Format::eEtc2R8G8B8A8UnormBlock:
		return 16;
	default:
		return 0;
	}
}
bool gx::internal::IsCompressed(gx::TextureFormat format)
{
	return format == gx::TextureFormat::CTXE || format == gx::TextureFormat::CTXE_A || format == gx::TextureFormat::CTXE_A4;
}
u64 gx::internal::GetSourceLayerSize(gx::TextureFormat format, vk::Format vkFormat, u32 width, u32 height)
{
	const u64 pixelCount = GetPixelCount(width, height);
	switch (format)
	{
	case gx::TextureFormat::A4:
	case gx::TextureFormat::L4:
		return (pixelCount + 1) / 2;
	case gx::TextureFormat::A8:
	case gx::TextureFormat::L8:
	case gx::TextureFormat::L4A4:
		return pixelCount;
	case gx::TextureFormat::L8A8:
	case gx::TextureFormat::R5G6B5:
	case gx::TextureFormat::R5G5B5A1:
	case gx::TextureFormat::R4G4B4A4:
		return pixelCount * 2;
	case gx::TextureFormat::R8G8B8A8:
		return pixelCount * 4;
	case gx::TextureFormat::CTXE:
	case gx::TextureFormat::CTXE_A:
	case gx::TextureFormat::CTXE_A4:
		return static_cast<u64>((width + 3u) / 4u) * ((height + 3u) / 4u) * GetCompressedBlockSize(vkFormat);
	default:
		return 0;
	}
}
u64 gx::internal::GetUploadLayerSize(gx::TextureFormat format, vk::Format vkFormat, u32 width, u32 height)
{
	const u64 pixelCount = GetPixelCount(width, height);
	if (format == gx::TextureFormat::A4 || format == gx::TextureFormat::L4)
	{
		return pixelCount;
	}
	if (format == gx::TextureFormat::L4A4 && vkFormat == vk::Format::eR8G8Unorm)
	{
		return pixelCount * 2;
	}
	return GetSourceLayerSize(format, vkFormat, width, height);
}
vk::ComponentMapping gx::internal::GetComponentMapping(gx::TextureFormat format)
{
	vk::ComponentMapping mapping = vk::ComponentMapping()
		.setR(vk::ComponentSwizzle::eIdentity)
		.setG(vk::ComponentSwizzle::eIdentity)
		.setB(vk::ComponentSwizzle::eIdentity)
		.setA(vk::ComponentSwizzle::eIdentity);
	if (format == gx::TextureFormat::A4 || format == gx::TextureFormat::A8)
	{
		mapping.setR(vk::ComponentSwizzle::eOne)
			.setG(vk::ComponentSwizzle::eOne)
			.setB(vk::ComponentSwizzle::eOne)
			.setA(vk::ComponentSwizzle::eR);
	}
	else if (format == gx::TextureFormat::L4 || format == gx::TextureFormat::L8)
	{
		mapping.setR(vk::ComponentSwizzle::eR)
			.setG(vk::ComponentSwizzle::eR)
			.setB(vk::ComponentSwizzle::eR)
			.setA(vk::ComponentSwizzle::eOne);
	}
	else if (format == gx::TextureFormat::L4A4 || format == gx::TextureFormat::L8A8)
	{
		mapping.setR(vk::ComponentSwizzle::eR)
			.setG(vk::ComponentSwizzle::eR)
			.setB(vk::ComponentSwizzle::eR)
			.setA(vk::ComponentSwizzle::eG);
	}
	return mapping;
}
u8 gx::internal::Expand4To8(u8 value)
{
	value &= 0x0F;
	return static_cast<u8>((value << 4) | value);
}
void gx::internal::Expand4BitSingleChannelData(const u8* source, u8* target, u64 pixelCount)
{
	for (u64 i = 0; i < pixelCount; ++i)
	{
		const u8 packed = source[i / 2];
		const u8 nibble = (i & 1) ? static_cast<u8>(packed >> 4) : static_cast<u8>(packed & 0x0F);
		target[i] = Expand4To8(nibble);
	}
}
void gx::internal::ExpandLA4Data(const u8* source, u8* target, u64 pixelCount)
{
	for (u64 i = 0; i < pixelCount; ++i)
	{
		const u8 packed = source[i];
		target[(i * 2) + 0] = Expand4To8(static_cast<u8>(packed & 0x0F));
		target[(i * 2) + 1] = Expand4To8(static_cast<u8>(packed >> 4));
	}
}