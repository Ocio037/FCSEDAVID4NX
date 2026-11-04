#include<Display.h>
#include"Data/DisplayData.h"
#include"GxUtil.h"
#include<Array.h>
#include<cstring>
#include<sstream>
gx::Display::Display() : Data(nullptr)
{

}
gx::Display::Display(const detail::DisplayInfo& Info) : Data(nullptr)
{
	Initialize(Info);
}
gx::Display::~Display()
{
	Finalize();
}
void gx::Display::Initialize(const detail::DisplayInfo& Info)
{
	Data = Mem::MakeUnique<DisplayData>();
	if (!Data) return;
	//Info
	{
		Data->Extent.setHeight(Info.Resolution.Height);
		Data->Extent.setWidth(Info.Resolution.Width);
		Data->DeviceContext.PhyDeviceDat.Resolution = Info.Resolution;
	}
	vk::ApplicationInfo AppInfo = vk::ApplicationInfo()
		.setApiVersion(VK_API_VERSION_1_1)
		.setApplicationVersion(VK_MAKE_VERSION(0, 0, 0))
		.setEngineVersion(VK_MAKE_VERSION(0, 0, 0))
		.setPApplicationName("GfxOhanaLib")
		.setPEngineName("GfxOhanaLib");
	DynamicArray<const char*> Extenciones;
	{
		Extenciones.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		Extenciones.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	}
	vk::InstanceCreateInfo InstCrtInfo = vk::InstanceCreateInfo()
		.setEnabledExtensionCount(Extenciones.size())
		.setPApplicationInfo(&AppInfo)
		.setPEnabledExtensionNames(Extenciones);
	Data->Instance = vk::createInstance(InstCrtInfo);
	VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {};
	{
		SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		SurfaceCreateInfo.hinstance = Info.hInstance;
		SurfaceCreateInfo.hwnd = Info.Hwnd;
	}
	vkCreateWin32SurfaceKHR(Data->Instance, &SurfaceCreateInfo, nullptr, &Data->Surface);
	const vk::SurfaceKHR surface{ Data->Surface };
	const auto physicalDevices = Data->Instance.enumeratePhysicalDevices();
	if (physicalDevices.empty())
	{
		return;
	}
	for (vk::PhysicalDevice physicalDevice : physicalDevices)
	{
		if (!internal::HasDeviceExtension(physicalDevice, VK_KHR_SWAPCHAIN_EXTENSION_NAME) ||
			!internal::HasPresentableGraphicsQueue(physicalDevice, surface) ||
			!internal::SupportsSingleSampleScene(physicalDevice) ||
			!internal::HasColorAttachmentSurfaceFormat(physicalDevice, surface) ||
			!(physicalDevice.getSurfaceCapabilitiesKHR(surface).supportedUsageFlags & vk::ImageUsageFlagBits::eColorAttachment))
		{
			continue;
		}
		Data->DeviceContext.PhyDeviceDat.PhyDevice = physicalDevice;
		break;
	}
	if (!Data->DeviceContext.PhyDeviceDat.PhyDevice)
	{
		return;
	}
	{
		const vk::Format defaultDepthFormats[] = {
			vk::Format::eD32Sfloat,
			vk::Format::eD32SfloatS8Uint,
			vk::Format::eD24UnormS8Uint,
			vk::Format::eD16Unorm,
		};
		constexpr u32 depthFormatCount = sizeof(defaultDepthFormats) / sizeof(defaultDepthFormats[0]);
		for (u32 formatIndex = 0; formatIndex < depthFormatCount && Data->DeviceContext.DeviceDat.DepthFormat == vk::Format::eUndefined; ++formatIndex)
		{
			const vk::Format format = defaultDepthFormats[formatIndex];
			const auto properties = Data->DeviceContext.PhyDeviceDat.PhyDevice.getFormatProperties(format);
			if ((properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) && (internal::GetImageSampleCounts(Data->DeviceContext.PhyDeviceDat.PhyDevice, format, vk::ImageUsageFlagBits::eDepthStencilAttachment) & vk::SampleCountFlagBits::e1))
			{
				Data->DeviceContext.DeviceDat.DepthFormat = format;
			}
		}
		if (Data->DeviceContext.DeviceDat.DepthFormat == vk::Format::eUndefined)
		{
			return;
		}
	}
	const auto QueueFamilyProp = Data->DeviceContext.PhyDeviceDat.PhyDevice.getQueueFamilyProperties();
	Data->DeviceContext.DeviceDat.GfxFamily = UINT32_MAX;
	for (size_t i = 0; i < QueueFamilyProp.size(); i++)
	{
		if ((QueueFamilyProp[i].queueFlags & vk::QueueFlagBits::eGraphics) && Data->DeviceContext.PhyDeviceDat.PhyDevice.getSurfaceSupportKHR(i, surface))
		{
			Data->DeviceContext.DeviceDat.GfxFamily = i;
		}
	}
	if (Data->DeviceContext.DeviceDat.GfxFamily == UINT32_MAX)
	{
		return;
	}
	const auto DeviceExtensionsAvailable = Data->DeviceContext.PhyDeviceDat.PhyDevice.enumerateDeviceExtensionProperties();
	bool HasPresentIdExtension = false;
	bool HasPresentWaitExtension = false;
	for (const vk::ExtensionProperties& Extension : DeviceExtensionsAvailable)
	{
		HasPresentIdExtension = HasPresentIdExtension ||
			std::strcmp(Extension.extensionName, VK_KHR_PRESENT_ID_EXTENSION_NAME) == 0;
		HasPresentWaitExtension = HasPresentWaitExtension ||
			std::strcmp(Extension.extensionName, VK_KHR_PRESENT_WAIT_EXTENSION_NAME) == 0;
	}
	const vk::PhysicalDeviceProperties physicalProperties = Data->DeviceContext.PhyDeviceDat.PhyDevice.getProperties();
	const auto surfaceFormats = Data->DeviceContext.PhyDeviceDat.PhyDevice.getSurfaceFormatsKHR(surface);
	if (surfaceFormats.empty())
	{
		return;
	}
	const Array<vk::Format, 4> preferredSurfaceFormats = {
		vk::Format::eB8G8R8A8Srgb,
		vk::Format::eR8G8B8A8Srgb,
		vk::Format::eB8G8R8A8Unorm,
		vk::Format::eR8G8B8A8Unorm,
	};
	vk::SurfaceFormatKHR selectedSurfaceFormat{};
	bool surfaceFormatFound = false;
	for (vk::Format preferredFormat : preferredSurfaceFormats)
	{
		const vk::FormatProperties formatProperties = Data->DeviceContext.PhyDeviceDat.PhyDevice.getFormatProperties(preferredFormat);
		if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment))
		{
			continue;
		}
		for (const vk::SurfaceFormatKHR& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == preferredFormat && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				selectedSurfaceFormat = surfaceFormat;
				surfaceFormatFound = true;
				break;
			}
		}
		if (surfaceFormatFound)
		{
			break;
		}
	}
	if (!surfaceFormatFound && surfaceFormats.size() == 1 && surfaceFormats.front().format == vk::Format::eUndefined)
	{
		for (vk::Format preferredFormat : preferredSurfaceFormats)
		{
			const vk::FormatProperties formatProperties = Data->DeviceContext.PhyDeviceDat.PhyDevice.getFormatProperties(preferredFormat);
			if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
			{
				selectedSurfaceFormat = vk::SurfaceFormatKHR();
				selectedSurfaceFormat.format = preferredFormat;
				selectedSurfaceFormat.colorSpace = surfaceFormats.front().colorSpace;
				surfaceFormatFound = true;
				break;
			}
		}
	}
	if (!surfaceFormatFound)
	{
		for (const vk::SurfaceFormatKHR& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == vk::Format::eUndefined)
			{
				continue;
			}
			const vk::FormatProperties formatProperties = Data->DeviceContext.PhyDeviceDat.PhyDevice.getFormatProperties(surfaceFormat.format);
			if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
			{
				selectedSurfaceFormat = surfaceFormat;
				surfaceFormatFound = true;
				break;
			}
		}
	}
	if (!surfaceFormatFound)
	{
		return;
	}
	Data->DeviceContext.DeviceDat.ImageFormat = selectedSurfaceFormat.format;
	Data->DeviceContext.DeviceDat.SceneFormat = vk::Format::eR8G8B8A8Unorm;
	bool PresentWaitEnabled = false;
	if (HasPresentIdExtension && HasPresentWaitExtension)
	{
		vk::PhysicalDevicePresentWaitFeaturesKHR PresentWaitSupport{};
		vk::PhysicalDevicePresentIdFeaturesKHR PresentIdSupport{};
		PresentIdSupport.setPNext(&PresentWaitSupport);
		vk::PhysicalDeviceFeatures2 SupportedFeatures{};
		SupportedFeatures.setPNext(&PresentIdSupport);
		Data->DeviceContext.PhyDeviceDat.PhyDevice.getFeatures2(&SupportedFeatures);
		PresentWaitEnabled = PresentIdSupport.presentId && PresentWaitSupport.presentWait;
	}
	const f32 Prioridad = 0.0f;
	const vk::DeviceQueueCreateInfo DeviceQueueCreateInfo = vk::DeviceQueueCreateInfo()
		.setQueueCount(0x1)
		.setQueueFamilyIndex(Data->DeviceContext.DeviceDat.GfxFamily)
		.setQueuePriorities(Prioridad);
	DynamicArray<const char*> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	vk::PhysicalDevicePresentWaitFeaturesKHR PresentWaitFeature{};
	vk::PhysicalDevicePresentIdFeaturesKHR PresentIdFeature{};
	if (PresentWaitEnabled)
	{
		DeviceExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
		DeviceExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
		PresentWaitFeature.setPresentWait(VK_TRUE);
		PresentIdFeature.setPresentId(VK_TRUE).setPNext(&PresentWaitFeature);
	}
	const vk::PhysicalDeviceFeatures AvailableFeatures = Data->DeviceContext.PhyDeviceDat.PhyDevice.getFeatures();
	vk::PhysicalDeviceFeatures EnabledFeatures{};
	EnabledFeatures.setTextureCompressionBC(AvailableFeatures.textureCompressionBC)
		.setTextureCompressionETC2(AvailableFeatures.textureCompressionETC2)
		.setTextureCompressionASTC_LDR(AvailableFeatures.textureCompressionASTC_LDR);
	vk::DeviceCreateInfo DeviceCreateInfo = vk::DeviceCreateInfo()
		.setPEnabledExtensionNames(DeviceExtensions)
		.setPEnabledFeatures(&EnabledFeatures)
		.setQueueCreateInfos(DeviceQueueCreateInfo);
	if (PresentWaitEnabled)
	{
		DeviceCreateInfo.setPNext(&PresentIdFeature);
	}
	Data->DeviceContext.DeviceDat.Device = Data->DeviceContext.PhyDeviceDat.PhyDevice.createDevice(DeviceCreateInfo);
	Data->DeviceContext.DeviceDat.Queue = Data->DeviceContext.DeviceDat.Device.getQueue(Data->DeviceContext.DeviceDat.GfxFamily, 0);
	Data->DeviceContext.DeviceDat.PresentWaitEnabled = PresentWaitEnabled;
	vk::SurfaceCapabilitiesKHR Capabilities = Data->DeviceContext.PhyDeviceDat.PhyDevice.getSurfaceCapabilitiesKHR(surface);
	Data->DeviceContext.DeviceDat.UseOffscreenScene = Info.UseOffscreenScene;
	if (Capabilities.currentExtent.width != UINT32_MAX)
	{
		Data->Extent = Capabilities.currentExtent;
	}
	else
	{
		const auto ClampExtent = [](u32 value, u32 minimum, u32 maximum)
		{
			return value < minimum ? minimum : value > maximum ? maximum : value;
		};
		Data->Extent.width = ClampExtent(Data->Extent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
		Data->Extent.height = ClampExtent(Data->Extent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);
	}
	Data->DeviceContext.PhyDeviceDat.Resolution.Width = Data->Extent.width;
	Data->DeviceContext.PhyDeviceDat.Resolution.Height = Data->Extent.height;
	u32 swapchainImageCount = Capabilities.minImageCount < 2 ? 2 : Capabilities.minImageCount;
	if (Capabilities.maxImageCount != 0 && swapchainImageCount > Capabilities.maxImageCount)
	{
		swapchainImageCount = Capabilities.maxImageCount;
	}
	const vk::CompositeAlphaFlagBitsKHR compositeAlphaPreferences[] = {
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
		vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
		vk::CompositeAlphaFlagBitsKHR::eInherit,
	};
	vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	bool compositeAlphaFound = false;
	for (vk::CompositeAlphaFlagBitsKHR candidate : compositeAlphaPreferences)
	{
		if (Capabilities.supportedCompositeAlpha & candidate)
		{
			compositeAlpha = candidate;
			compositeAlphaFound = true;
			break;
		}
	}
	if (!compositeAlphaFound)
	{
		return;
	}
	{
		std::ostringstream profile;
		profile << "Foca 480p AA profile: single-sample Vulkan 1.1 path, three-position shader reconstruction"
			<< ", scene target=RGBA8"
			<< ", adapter=" << physicalProperties.deviceName << "\r\n";
		OutputDebugStringA(profile.str().c_str());
	}
	vk::SwapchainCreateInfoKHR SwapchainCreateInfo = vk::SwapchainCreateInfoKHR()
		.setClipped(VK_TRUE)
		.setCompositeAlpha(compositeAlpha)
		.setImageArrayLayers(1)
		.setImageColorSpace(selectedSurfaceFormat.colorSpace)
		.setImageExtent(Data->Extent)
		.setImageFormat(Data->DeviceContext.DeviceDat.ImageFormat)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setMinImageCount(swapchainImageCount)
		.setPresentMode(vk::PresentModeKHR::eFifo)
		.setPreTransform(Capabilities.currentTransform)
		.setSurface(surface)
		.setOldSwapchain(nullptr); 
	Data->DeviceContext.DeviceDat.Swapchain = Data->DeviceContext.DeviceDat.Device.createSwapchainKHR(SwapchainCreateInfo);
	Data->DeviceContext.DeviceDat.Device.getSwapchainImagesKHR(Data->DeviceContext.DeviceDat.Swapchain, &Data->ImageCount, static_cast<vk::Image*>(nullptr));
	Data->Images.resize(Data->ImageCount);
	Data->DeviceContext.DeviceDat.Device.getSwapchainImagesKHR(Data->DeviceContext.DeviceDat.Swapchain, &Data->ImageCount, Data->Images.data());
	Data->DeviceContext.DeviceDat.ImagesViews.resize(Data->ImageCount);
	for (u32 i = 0; i < Data->ImageCount; i++) 
	{
		vk::ImageSubresourceRange ImgSubSrcRange = vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseArrayLayer(0)
			.setBaseMipLevel(0)
			.setLayerCount(1)
			.setLevelCount(1);
		vk::ImageViewCreateInfo ImgViewCrtInfo = vk::ImageViewCreateInfo()
			.setFormat(Data->DeviceContext.DeviceDat.ImageFormat)
			.setImage(Data->Images[i])
			.setSubresourceRange(ImgSubSrcRange)
			.setViewType(vk::ImageViewType::e2D);
		Data->DeviceContext.DeviceDat.ImagesViews[i] = Data->DeviceContext.DeviceDat.Device.createImageView(ImgViewCrtInfo);
	}
}
void gx::Display::Finalize()
{
	if (Data)
	{
		if (Data->DeviceContext.DeviceDat.Device)
		{
			Data->DeviceContext.DeviceDat.Device.waitIdle();
			for (vk::ImageView ImageView : Data->DeviceContext.DeviceDat.ImagesViews)
			{
				Data->DeviceContext.DeviceDat.Device.destroyImageView(ImageView);
			}
			Data->DeviceContext.DeviceDat.ImagesViews.clear();
			if (Data->DeviceContext.DeviceDat.Swapchain)
			{
				Data->DeviceContext.DeviceDat.Device.destroySwapchainKHR(Data->DeviceContext.DeviceDat.Swapchain);
			}
			Data->DeviceContext.DeviceDat.Device.destroy();
		}
		if (Data->Surface)
		{
			Data->Instance.destroySurfaceKHR(vk::SurfaceKHR{ Data->Surface });
		}
		if (Data->Instance)
		{
			Data->Instance.destroy();
		}
		Data.reset();
	}
}
const gx::internal::Device* gx::Display::GetDeviceInfo() const
{
	return &Data->DeviceContext;
}
gx::detail::DisplayResolution gx::Display::GetResolution() const
{
	return Data ? Data->DeviceContext.PhyDeviceDat.Resolution : detail::DisplayResolution{};
}