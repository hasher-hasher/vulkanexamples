
/*
	render the viewport into a texture to be used in the scene
*/
#include "VulkanInitializer.h"
#include "Helpers.cpp"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

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
	uint32_t swapchainCurrentImageIndex = 0;
	std::vector<VkImage> swapchainImages = {};
	std::vector<VkImageView> imageViews = {};
	std::vector<VkFramebuffer> frameBuffers = {};

	/*
		offscreen related
	*/
	VkRenderPass offscreenRenderpass = VK_NULL_HANDLE;
	VkImage offscreenTextureImage;
	VkDeviceMemory offscreenTextureImageMemory = VK_NULL_HANDLE;
	VkImageView offscreenImageView = VK_NULL_HANDLE;
	VkSampler offscreenSampler = VK_NULL_HANDLE;
	VkFramebuffer offscreenFramebuffer = VK_NULL_HANDLE;

	// descritors
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;

	VkRenderPass renderPass = VK_NULL_HANDLE;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers = {};

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	std::vector<VkSemaphore> swapchainProcessImageSemaphores = {};
	std::vector<VkSemaphore> swapchainReadyToPresentSemaphores = {};
	std::vector<VkFence> swapchainFrameFance = {};

	uint32_t currentFrame = 0;

	struct Buffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
	};

	Buffer vertexBuffer = {};
	VkDeviceSize vertexBufferSize = sizeof(Vertex) * 3;

	struct Vertex {
		glm::vec2 pos;
		glm::vec3 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = static_cast<uint32_t>(sizeof(Vertex));
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			return attributeDescriptions;
		}
	};

	std::vector<Vertex> vertices = {
		{{0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	};

	ViewportToTexture(SDL_Window* window, VulkanInitializer* vulkanInitializer) {
		m_Window = window;
		m_vulkanInitializer = vulkanInitializer;

		// swapchain related
		CreateSwapchain();
		CreateSwapchainImageViews();
		CreateTextureImage();
		CreateTextureImageView();

		// offset related
		CreateOffscreenTextureResources();
		CreateOffscreenRenderPass();
		CreateOffscreenFramebuffer();

		CreateRenderPass();
		CreateFrameBuffers();
		CreateCommandPool();
		CreateCommandBuffers();
		CreateGraphicsPipeline();

		CreateSynchObjects();

		CreateVertexBuffer();
	}
	~ViewportToTexture() {
		vkDeviceWaitIdle(m_vulkanInitializer->device);

		vkDestroyBuffer(m_vulkanInitializer->device, vertexBuffer.buffer, nullptr);
		vkFreeMemory(m_vulkanInitializer->device, vertexBuffer.bufferMemory, nullptr);

		for (int i = 0; i < swapchainImageCount; i++) {
			vkDestroySemaphore(m_vulkanInitializer->device, swapchainProcessImageSemaphores[i], nullptr);
			vkDestroySemaphore(m_vulkanInitializer->device, swapchainReadyToPresentSemaphores[i], nullptr);
			vkDestroyFence(m_vulkanInitializer->device, swapchainFrameFance[i], nullptr);
		}

		vkDestroyPipelineLayout(m_vulkanInitializer->device, pipelineLayout, nullptr);
		vkDestroyPipeline(m_vulkanInitializer->device, pipeline, nullptr);

		vkDestroyCommandPool(m_vulkanInitializer->device, commandPool, nullptr);

		for (auto& framebuffer : frameBuffers) {
			vkDestroyFramebuffer(m_vulkanInitializer->device, framebuffer, nullptr);
		}
		frameBuffers.clear();

		vkDestroyRenderPass(m_vulkanInitializer->device, renderPass, nullptr);

		vkDestroyImage(m_vulkanInitializer->device, textureImage, nullptr);
		vkFreeMemory(m_vulkanInitializer->device, textureImageMemory, nullptr);
		vkDestroyImageView(m_vulkanInitializer->device, textureImageView, nullptr);

		for (auto& imageView : imageViews) {
			vkDestroyImageView(m_vulkanInitializer->device, imageView, nullptr);
		}
		imageViews.clear();

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

	void CreateSwapchainImageViews() {
		for (auto &swapchainImage : swapchainImages) {
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = swapchainImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = surfaceFormat.format;
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			VkImageView imageView;
			VkResult res = vkCreateImageView(m_vulkanInitializer->device, &viewInfo, nullptr, &imageView);
			ASSERT(res, "failed to create texture image view!");

			imageViews.push_back(imageView);
		}
	}

	void CreateTextureImage() {
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = surfaceFormat.format;
		imageCreateInfo.extent.width = extent2D.width;
		imageCreateInfo.extent.height = extent2D.height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
								VK_IMAGE_USAGE_TRANSFER_DST_BIT |
								VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult res = vkCreateImage(m_vulkanInitializer->device, &imageCreateInfo, nullptr, &textureImage);
		ASSERT(res, "Unable to create image for texture.");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_vulkanInitializer->device, textureImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		res = vkAllocateMemory(m_vulkanInitializer->device, &allocInfo, nullptr, &textureImageMemory);
		ASSERT(res, "failed to allocate image memory!");

		vkBindImageMemory(m_vulkanInitializer->device, textureImage, textureImageMemory, 0);
	}

	void CreateTextureImageView() {
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = textureImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = surfaceFormat.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		
		VkResult res = vkCreateImageView(m_vulkanInitializer->device, &imageViewCreateInfo, nullptr, &textureImageView);
		ASSERT(res, "Unable to create image view for texture.");
	}

	void CreateOffscreenTextureResources() {
		/*
			create offscreen image
		*/
		{
			// Color attachment
			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.format = surfaceFormat.format;
			image.extent.width = extent2D.width;
			image.extent.height = extent2D.height;
			image.extent.depth = 1;
			image.mipLevels = 1;
			image.arrayLayers = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			// We will sample directly from the color attachment
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			VkMemoryAllocateInfo memAlloc = {};
			VkMemoryRequirements memReqs;

			ASSERT(vkCreateImage(m_vulkanInitializer->device, &image, nullptr, &offscreenTextureImage));
			vkGetImageMemoryRequirements(m_vulkanInitializer->device, offscreenTextureImage, &memReqs);
			memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			ASSERT(vkAllocateMemory(m_vulkanInitializer->device, &memAlloc, nullptr, &offscreenTextureImageMemory));
			ASSERT(vkBindImageMemory(m_vulkanInitializer->device, offscreenTextureImage, offscreenTextureImageMemory, 0));
		}

		/*
			create offscreen imageView
		*/
		{
			VkImageViewCreateInfo colorImageView = {};
			colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = surfaceFormat.format;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreenTextureImage;
			ASSERT(vkCreateImageView(m_vulkanInitializer->device, &colorImageView, nullptr, &offscreenImageView));
		}

		/*
			create offscreen sampler
		*/
		{
			VkSamplerCreateInfo samplerInfo = {};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = samplerInfo.addressModeU;
			samplerInfo.addressModeW = samplerInfo.addressModeU;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.maxAnisotropy = 1.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 1.0f;
			samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			ASSERT(vkCreateSampler(m_vulkanInitializer->device, &samplerInfo, nullptr, &offscreenSampler));
		}
	}

	void CreateOffscreenRenderPass() {
		VkAttachmentDescription attachmentDescription = {};
		attachmentDescription.format = surfaceFormat.format;
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference colorAttachmentReference = {};
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorAttachmentReference;

		VkSubpassDependency subpassDependency{};
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &attachmentDescription;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;

		VkResult res = vkCreateRenderPass(m_vulkanInitializer->device, &renderPassCreateInfo, nullptr, &offscreenRenderpass);
		ASSERT(res, "failed to create render pass");
	}

	void CreateOffscreenFramebuffer() {
		VkImageView attachments = offscreenImageView;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.renderPass = offscreenRenderpass;
		fbufCreateInfo.attachmentCount = 1;
		fbufCreateInfo.pAttachments = &attachments;
		fbufCreateInfo.width = extent2D.width;
		fbufCreateInfo.height = extent2D.height;
		fbufCreateInfo.layers = 1;

		ASSERT(vkCreateFramebuffer(m_vulkanInitializer->device, &fbufCreateInfo, nullptr, &offscreenFramebuffer));
	}

	void CreateRenderPass() {
		VkAttachmentDescription attachmentDescription = {};
		attachmentDescription.format = surfaceFormat.format;
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentReference = {};
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorAttachmentReference;

		VkSubpassDependency subpassDependency{};
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &attachmentDescription;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;

		VkResult res = vkCreateRenderPass(m_vulkanInitializer->device, &renderPassCreateInfo, nullptr, &renderPass);
		ASSERT(res, "failed to create render pass");
	}

	void CreateFrameBuffers() {
		for (auto& imageView : imageViews) {
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = 1;
			framebufferCreateInfo.pAttachments = &imageView;
			framebufferCreateInfo.width = extent2D.width;
			framebufferCreateInfo.height = extent2D.height;
			framebufferCreateInfo.layers = 1;

			VkFramebuffer frameBuffer;
			ASSERT(vkCreateFramebuffer(m_vulkanInitializer->device, &framebufferCreateInfo, nullptr, &frameBuffer), "failed to create framebuffer.");

			frameBuffers.push_back(frameBuffer);
		}
	}

	void CreateCommandPool() {
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = m_vulkanInitializer->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		ASSERT(vkCreateCommandPool(m_vulkanInitializer->device, &commandPoolCreateInfo, nullptr, &commandPool), "failed to create command pool.");
	}

	void CreateCommandBuffers() {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = swapchainImageCount;

		commandBuffers.resize(swapchainImageCount);
		ASSERT(vkAllocateCommandBuffers(m_vulkanInitializer->device, &allocInfo, commandBuffers.data()));
	}

	void CreateGraphicsPipeline() {
		std::vector<char> vertShaderCode = readShaderFile("../Shaders/vert.spv");
		std::vector<char> fragShaderCode = readShaderFile("../Shaders/frag.spv");

		VkShaderModule vertShaderModule = createShaderModule(m_vulkanInitializer->device, vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(m_vulkanInitializer->device, fragShaderCode);

		// vertex pipeline creation
		VkPipelineShaderStageCreateInfo pipelineVertShaderStageCreateInfo = {};
		pipelineVertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineVertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineVertShaderStageCreateInfo.module = vertShaderModule;
		pipelineVertShaderStageCreateInfo.pName = "main";

		// fragment pipeline creation
		VkPipelineShaderStageCreateInfo pipelineFragShaderStageCreateInfo = {};
		pipelineFragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineFragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipelineFragShaderStageCreateInfo.module = fragShaderModule;
		pipelineFragShaderStageCreateInfo.pName = "main";

		// creating stages for shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages{ pipelineVertShaderStageCreateInfo , pipelineFragShaderStageCreateInfo };

		// vertex input
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
		pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

		// viewport
		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = extent2D.width;
		viewport.height = extent2D.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// scissor
		VkRect2D rect2D = {};
		rect2D.offset = VkOffset2D{ 0, 0 };
		rect2D.extent = extent2D;

		// viewport
		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
		pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.viewportCount = 1;
		pipelineViewportStateCreateInfo.pViewports = &viewport;
		pipelineViewportStateCreateInfo.scissorCount = 1;
		pipelineViewportStateCreateInfo.pScissors = &rect2D;

		// rasterizer
		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		//pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		//pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

		// multisample
		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
		pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.pNext = nullptr;
		pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;

		// color blend stuff
		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
		pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
		pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
														   VK_COLOR_COMPONENT_G_BIT |
														   VK_COLOR_COMPONENT_B_BIT |
														   VK_COLOR_COMPONENT_A_BIT;

		// Color blend
		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
		pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
		pipelineColorBlendStateCreateInfo.attachmentCount = 1;
		pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;
		pipelineColorBlendStateCreateInfo.blendConstants[0] = VK_BLEND_FACTOR_ZERO;
		pipelineColorBlendStateCreateInfo.blendConstants[1] = VK_BLEND_FACTOR_ZERO;
		pipelineColorBlendStateCreateInfo.blendConstants[2] = VK_BLEND_FACTOR_ZERO;
		pipelineColorBlendStateCreateInfo.blendConstants[3] = VK_BLEND_FACTOR_ZERO;

		// pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 0;
		pipelineLayoutCreateInfo.pSetLayouts = nullptr;

		ASSERT(vkCreatePipelineLayout(m_vulkanInitializer->device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout), "failed to create pipeline layout.");

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.pNext = nullptr;
		graphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		graphicsPipelineCreateInfo.pStages = shaderStages.data();
		graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
		graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
		graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
		graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
		graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
		graphicsPipelineCreateInfo.layout = pipelineLayout;
		graphicsPipelineCreateInfo.renderPass = renderPass;
		graphicsPipelineCreateInfo.subpass = 0;

		ASSERT(vkCreateGraphicsPipelines(m_vulkanInitializer->device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline), "failed to create graphics pipeline.");

		vkDestroyShaderModule(m_vulkanInitializer->device, vertShaderModule, nullptr);
		vkDestroyShaderModule(m_vulkanInitializer->device, fragShaderModule, nullptr);
	}

	void CreateSynchObjects() {
		// each of those objects will be retrieved per frame
		swapchainProcessImageSemaphores.resize(swapchainImageCount);
		swapchainReadyToPresentSemaphores.resize(swapchainImageCount);
		swapchainFrameFance.resize(swapchainImageCount);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < swapchainImageCount; i++) {
			ASSERT(vkCreateSemaphore(m_vulkanInitializer->device, &semaphoreInfo, nullptr, &swapchainProcessImageSemaphores[i]), "error creating semaphore");
			ASSERT(vkCreateSemaphore(m_vulkanInitializer->device, &semaphoreInfo, nullptr, &swapchainReadyToPresentSemaphores[i]), "error creating semaphore");
			ASSERT(vkCreateFence(m_vulkanInitializer->device, &fenceInfo, nullptr, &swapchainFrameFance[i]), "error creating fence");
		}
	}

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(m_vulkanInitializer->physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = m_vulkanInitializer->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);

		ASSERT(vkCreateBuffer(m_vulkanInitializer->device, &bufferInfo, nullptr, &buffer), "failed to create vertex buffer.");

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_vulkanInitializer->device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

		ASSERT(vkAllocateMemory(m_vulkanInitializer->device, &allocInfo, nullptr, &bufferMemory), "failed to allocate vertex buffer memory!");

		vkBindBufferMemory(m_vulkanInitializer->device, buffer, bufferMemory, 0);
	}

	void CreateVertexBuffer() {
		CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer.buffer, vertexBuffer.bufferMemory);

		void* data;
		vkMapMemory(m_vulkanInitializer->device, vertexBuffer.bufferMemory, 0, sizeof(vertices[0]) * vertices.size(), 0, &data);
		memcpy(data, vertices.data(), (size_t)sizeof(vertices[0]) * vertices.size());
		vkUnmapMemory(m_vulkanInitializer->device, vertexBuffer.bufferMemory);
	}

	void ChangeLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		BeginOneTimeCommandBuffer(m_vulkanInitializer->device, commandPool, commandBuffer);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndOneTimeCommandBuffer(m_vulkanInitializer->device, m_vulkanInitializer->queue, commandPool, commandBuffer);
	}

	void Draw() {
		// Startint the draw
		ASSERT(vkAcquireNextImageKHR(m_vulkanInitializer->device, swapchain, UINT64_MAX, swapchainProcessImageSemaphores[currentFrame], VK_NULL_HANDLE, &swapchainCurrentImageIndex));

		vkWaitForFences(m_vulkanInitializer->device, 1, &swapchainFrameFance[currentFrame], VK_TRUE, UINT16_MAX);
		vkResetFences(m_vulkanInitializer->device, 1, &swapchainFrameFance[currentFrame]);

		// start to write on command buffer
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		ASSERT(vkBeginCommandBuffer(commandBuffers[swapchainCurrentImageIndex], &commandBufferBeginInfo), "couldn't start commandBuffer");

		/*
			First renderpass: offscreen
		*/

		//{
		//	// the cleaning screen for the offscreen should be red to be easy to see
		//	VkClearValue clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };

		//	VkRenderPassBeginInfo renderPassBeginInfo = {};
		//	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		//	renderPassBeginInfo.renderPass = renderPass;
		//	renderPassBeginInfo.framebuffer = frameBuffers[swapchainCurrentImageIndex];
		//	renderPassBeginInfo.renderArea.extent.width = extent2D.width;
		//	renderPassBeginInfo.renderArea.extent.height = extent2D.height;
		//	renderPassBeginInfo.clearValueCount = 1;
		//	renderPassBeginInfo.pClearValues = &clearColor;

		//	vkCmdBeginRenderPass(commandBuffers[swapchainCurrentImageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		//}

		// start presentation renderpass
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers[swapchainCurrentImageIndex];
		renderPassBeginInfo.renderArea.extent.width = extent2D.width;
		renderPassBeginInfo.renderArea.extent.height = extent2D.height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffers[swapchainCurrentImageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[swapchainCurrentImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffers[swapchainCurrentImageIndex], 0, 1, &vertexBuffer.buffer, &offset);
		vkCmdDraw(commandBuffers[swapchainCurrentImageIndex], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[swapchainCurrentImageIndex]);

		ASSERT(vkEndCommandBuffer(commandBuffers[swapchainCurrentImageIndex]), "couldn't end commandBuffer");

		// finish and send to presentation queue
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &swapchainProcessImageSemaphores[currentFrame];
		submitInfo.pWaitDstStageMask = &wait_stage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[swapchainCurrentImageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &swapchainReadyToPresentSemaphores[currentFrame];

		ASSERT(vkQueueSubmit(m_vulkanInitializer->queue, 1, &submitInfo, swapchainFrameFance[currentFrame]), "failed to submit to queue.");

		// now present
		VkPresentInfoKHR presentInfoKHR = {};
		presentInfoKHR.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfoKHR.waitSemaphoreCount = 1;
		presentInfoKHR.pWaitSemaphores = &swapchainReadyToPresentSemaphores[currentFrame];
		presentInfoKHR.swapchainCount = 1;
		presentInfoKHR.pSwapchains = &swapchain;
		presentInfoKHR.pImageIndices = &swapchainCurrentImageIndex;
		ASSERT(vkQueuePresentKHR(m_vulkanInitializer->queue, &presentInfoKHR), "failed to send to present queue.");

		currentFrame = (currentFrame + 1) % swapchainImageCount;
	}

	// Auxiliary functions
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(m_vulkanInitializer->physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}
};
