#include <iostream>
#include "SDL.h"

#include "ViewportToTexture.cpp"

int main(int argc, char* argv[]) {
	// setting up SDL
	SDL_Window *window = nullptr;

	int windowWidth = 500;
	int windowHeight = 500;

	// initializing just video for now
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		throw std::runtime_error("failed to initialize SDL video module");
	}

	// creating window
	window = SDL_CreateWindow(
		"Game Engine 2D",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		windowWidth,
		windowHeight,
		SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	if (!window) {
		throw std::runtime_error("failed to create SDL window");
	}

	VulkanInitializer vulkanInitializer = VulkanInitializer(window);

	ViewportToTexture viewportToTexture = ViewportToTexture(window, &vulkanInitializer);

	// main loop
	SDL_Event eventInfo;
	bool isApplicationRunning = true;

	while (isApplicationRunning) {
		while (SDL_PollEvent(&eventInfo)) {
			switch (eventInfo.type) {
			case SDL_QUIT:
				isApplicationRunning = false;
			}
		}
	}

	SDL_Quit();

	return 0;
}