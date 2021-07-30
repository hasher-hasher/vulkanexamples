/*
	This example renders the viewport into a texture and display it
*/
#pragma once

#include <iostream>
#include <vector>
#include <array>

#include "SDL.h"
#include "SDL_vulkan.h"
#include "vulkan/vulkan.h"

// function for handling validation layer
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::cout << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

class VulkanInitializer
{
public:
	VulkanInitializer(SDL_Window *window);
	~VulkanInitializer();

	//void createInstance(SDL_Window* window);
private:
	// enable validation layers
	bool validationLayer = true;

	std::vector<const char*> instanceLayers = {
		"VK_LAYER_KHRONOS_validation" // validation layer
	};
	VkDebugUtilsMessengerEXT cb = VK_NULL_HANDLE;

	// insert the string or constants for instance extentions to be enabled
	std::vector<const char*> instanceExtensions = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME, // debug extension
	};

	// logical device extensions
	std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		//VK_KHR_MAINTENANCE3_EXTENSION_NAME,  // add the array texture feature in the frag shader
		//VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME  // add the array texture feature in the frag shader
	};

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;

	// functions
	void ASSERT(VkResult result, const char* message);

	void CreateInstance(SDL_Window* window);
	void CreateValidationLayer();
	void CreateSurface(SDL_Window* window);
	void SelectPhysicalDevice();
	void CreateLogicalDevice();
	uint32_t getQueueFamilyIndex(VkQueueFlagBits queueFlagBits);
	void SelectQueue();
};
