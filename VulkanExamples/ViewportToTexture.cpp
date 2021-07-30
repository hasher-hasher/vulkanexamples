/*
	render the viewport into a texture to be used in the scene
*/
#include "VulkanInitializer.h"

class ViewportToTexture {
public:
	VulkanInitializer* m_vulkanInitializer;
	SDL_Window* m_Window = nullptr;

	VkFormat desiredFormat = VK_FORMAT_B8G8R8A8_UNORM;
	VkColorSpaceKHR desiredColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	VkPresentModeKHR desiredPresentation = VK_PRESENT_MODE_MAILBOX_KHR;

	VkSurfaceFormatKHR surfaceFormat = {};
	VkExtent2D extent2D = {};
	VkPresentModeKHR presentationMode = {};

	uint32_t swapchainImageCount = 2;
	VkSwapchainKHR swapchain = NULL;
	std::vector<VkImage> swapchainImages = {};

	ViewportToTexture(SDL_Window* window, VulkanInitializer* vulkanInitializer) {
		m_Window = window;
		m_vulkanInitializer = vulkanInitializer;

		CreateSwapchain();
	}
	~ViewportToTexture() {
		vkDestroySwapchainKHR(m_vulkanInitializer->device, swapchain, nullptr);
	}

	void CreateSwapchain() {
		// accepted format and color space formats by swapchain
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkanInitializer->physicalDevice, m_vulkanInitializer->surface, &formatCount, nullptr);
		ASSERT(formatCount, "failed to get supported format from swapchain.");
		std::vector<VkSurfaceFormatKHR> formats = {};
		formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkanInitializer->physicalDevice, m_vulkanInitializer->surface, &formatCount, formats.data());

		// are the desired formats available?
		for (const auto& availableFormat : formats) {
			if (availableFormat.format == desiredFormat && availableFormat.colorSpace == desiredColorSpace) {
				surfaceFormat = availableFormat;
				break;
			}
		}

		if (surfaceFormat.format == NULL) {
			std::runtime_error("surface format not available.");
		}

		// surface capabilities
		VkSurfaceCapabilitiesKHR surfaceCapabilitiesKHR = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vulkanInitializer->physicalDevice, m_vulkanInitializer->surface, &surfaceCapabilitiesKHR);
		// this problem might happen with mac. If yes, study to check how to solve
		if (surfaceCapabilitiesKHR.currentExtent.width == UINT32_MAX) {
			throw std::runtime_error("pixels dont fit in the screen.");
		}

		// setting the size of creation for the swapchain
		extent2D = surfaceCapabilitiesKHR.currentExtent;

		// presentation supported by the swapchain
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_vulkanInitializer->physicalDevice, m_vulkanInitializer->surface, &presentModeCount, nullptr);
		ASSERT(presentModeCount, "failed to get supported presentation mode from swapchain.");
		std::vector<VkPresentModeKHR> presentModes = {};
		presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_vulkanInitializer->physicalDevice, m_vulkanInitializer->surface, &presentModeCount, presentModes.data());

		VkPresentModeKHR presentModeKHR = {};
		for (const auto& availablePresentMode : presentModes) {
			if (availablePresentMode == desiredPresentation) {
				presentationMode = availablePresentMode;
			}
		}
		if (!presentationMode) {
			throw std::runtime_error("presentation mode not available");
		}

		// is surface compatible?
		// check if the current surface is supported by the queue family
		VkBool32 hasSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(
			m_vulkanInitializer->physicalDevice,
			m_vulkanInitializer->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT),
			m_vulkanInitializer->surface,
			&hasSupport
		);

		if (!hasSupport) {
			throw std::runtime_error("invalid surface.");
		}

		VkSwapchainCreateInfoKHR swapchainCreateInfoKHR = {};
		swapchainCreateInfoKHR.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfoKHR.surface = m_vulkanInitializer->surface;
		swapchainCreateInfoKHR.minImageCount = swapchainImageCount;
		swapchainCreateInfoKHR.imageFormat = surfaceFormat.format;
		swapchainCreateInfoKHR.imageColorSpace = surfaceFormat.colorSpace;
		swapchainCreateInfoKHR.imageExtent = extent2D;
		swapchainCreateInfoKHR.imageArrayLayers = 1;
		swapchainCreateInfoKHR.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfoKHR.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // possible flaw. If some problem with swapchain, see previous implementation
		swapchainCreateInfoKHR.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfoKHR.presentMode = presentationMode;
		swapchainCreateInfoKHR.clipped = VK_TRUE;
		swapchainCreateInfoKHR.oldSwapchain = VK_NULL_HANDLE;

		VkResult res = vkCreateSwapchainKHR(m_vulkanInitializer->device, &swapchainCreateInfoKHR, nullptr, &swapchain);
		ASSERT(res, "failed to create swap chain!");

		// retrieving images from swapchain after creation
		vkGetSwapchainImagesKHR(m_vulkanInitializer->device, swapchain, &swapchainImageCount, nullptr);
		swapchainImages.resize(swapchainImageCount);
		vkGetSwapchainImagesKHR(m_vulkanInitializer->device, swapchain, &swapchainImageCount, swapchainImages.data());
	}
};
