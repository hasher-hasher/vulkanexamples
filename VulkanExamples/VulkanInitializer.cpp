#include "VulkanInitializer.h"

VulkanInitializer::VulkanInitializer(SDL_Window* window)
{
	CreateInstance(window);
	CreateValidationLayer();
	CreateSurface(window);
	SelectPhysicalDevice();
	CreateLogicalDevice();
	SelectQueue();
}

VulkanInitializer::~VulkanInitializer()
{
	// destroy validation layer
	PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkDestroyDebugUtilsMessengerEXT");

	pfnDestroyDebugUtilsMessengerEXT(instance, cb, nullptr);

	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance, surface, nullptr);

	vkDestroyInstance(instance, nullptr);
}

void VulkanInitializer::ASSERT(VkResult result, const char *message = nullptr)
{
	if (result != VK_SUCCESS) {
		std::runtime_error(message + result);
	}
}

void VulkanInitializer::CreateInstance(SDL_Window* window)
{
	// getting nedded extensions from SDL
	unsigned int count;
	SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);

	std::vector<const char*> extensions;
	extensions.resize(count);

	SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());

	// appending found extensions from SDL to main extensions variable
	instanceExtensions.insert(instanceExtensions.end(), extensions.begin(), extensions.end());

	// configuring application structs
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = nullptr;
	applicationInfo.pApplicationName = "Game Engine";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "Game Engine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instancecCreateInfo{};
	instancecCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instancecCreateInfo.flags = 0;
	instancecCreateInfo.pApplicationInfo = &applicationInfo;

	if (validationLayer) {
		instancecCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
		instancecCreateInfo.ppEnabledLayerNames = instanceLayers.data();

		VkDebugUtilsMessengerCreateInfoEXT callback = {};
		callback = {};
		callback.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		callback.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		callback.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		callback.pfnUserCallback = debugCallback;
		callback.pUserData = nullptr;
		instancecCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&callback;
	}
	else {
		instancecCreateInfo.enabledLayerCount = 0;
	}

	instancecCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instancecCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	
	ASSERT(vkCreateInstance(&instancecCreateInfo, nullptr, &instance), "failed to create Vulkan instance.");
}

void VulkanInitializer::CreateValidationLayer()
{
	VkDebugUtilsMessengerCreateInfoEXT callback = {};
	callback.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	callback.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	callback.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	callback.pfnUserCallback = debugCallback;
	callback.pUserData = nullptr;

	PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	if (pfnCreateDebugUtilsMessengerEXT != nullptr) {
		pfnCreateDebugUtilsMessengerEXT(instance, &callback, nullptr, &cb);
	}
	else {
		throw std::runtime_error("failed to create DebugUtilsMessenger. Is the validation layer extension present in the project?");
	}
}

void VulkanInitializer::CreateSurface(SDL_Window* window)
{
	if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE) {
		throw std::runtime_error("failed to create surface.");
	}
}

void VulkanInitializer::SelectPhysicalDevice()
{
	// getting all available physical devices
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
	std::vector<VkPhysicalDevice> availablePhysicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, availablePhysicalDevices.data());

	struct Rank {
		int idx = 0;
		int score = 0;
	};
	Rank rank = {};

	for (int i = 0; i < availablePhysicalDevices.size(); i++) {
		VkPhysicalDeviceProperties physicalDeviceProperties = {};
		vkGetPhysicalDeviceProperties(availablePhysicalDevices[i], &physicalDeviceProperties);

		int score = 0;

		// set the metrics to choose the best graphics fits below
		switch (physicalDeviceProperties.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			score = 4;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			score = 3;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			score = 2;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			score = 1;
			break;
		default:
			break;
		}

		if (rank.score < score) {
			rank.score = score;
			rank.idx = i;
		}

		if (validationLayer) {
			std::cout << "available physical device: " << physicalDeviceProperties.deviceName << std::endl;
		}
	}

	if (rank.score == 0) {
		// VK_PHYSICAL_DEVICE_TYPE_OTHER was selected. Should crash
		throw std::runtime_error("failed to find a suitable graphic device.");
	}

	physicalDevice = availablePhysicalDevices[rank.idx];
}

void VulkanInitializer::CreateLogicalDevice()
{
	// queue
	std::vector<float> queuePriorities = { 1.0f };
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.queueFamilyIndex = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
	// creating just one queue since graphics supports all kind of operations
	deviceQueueCreateInfo.queueCount = static_cast<uint32_t>(1);
	deviceQueueCreateInfo.pQueuePriorities = queuePriorities.data();

	std::array<VkDeviceQueueCreateInfo, 1> deviceQueueCreateInfos = { deviceQueueCreateInfo };

	// indexing feature for dynamically create array of textures for shader
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{};
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	indexingFeatures.pNext = nullptr;

	// enabling sampler anisotropy
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkPhysicalDeviceFeatures2 physical_features2 = {};
	physical_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physical_features2.pNext = &indexingFeatures;
	physical_features2.features = deviceFeatures;

	vkGetPhysicalDeviceFeatures2(physicalDevice, &physical_features2);

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &physical_features2;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	ASSERT(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "failed to create logical device");
}

uint32_t VulkanInitializer::getQueueFamilyIndex(VkQueueFlagBits queueFlagBits)
{
	uint32_t pQueueFamilyPropertyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &pQueueFamilyPropertyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(pQueueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &pQueueFamilyPropertyCount, queueFamilyProperties.data());

	for (int i = 0; i < queueFamilyProperties.size(); i++) {
		if (queueFamilyProperties[i].queueFlags & queueFlagBits) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable queue family");
}

void VulkanInitializer::SelectQueue()
{
	// not accepting arguments on purpose. Always using graphics queue
	vkGetDeviceQueue(device, getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT), 0, &queue);
}
