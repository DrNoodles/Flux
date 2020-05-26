#pragma once

#include "AppTypes.h"
#include "AssImpModelLoaderService.h"
#include "FpsCounter.h"
#include "UI/UiPresenter.h"

#include <Renderer/CubemapTextureLoader.h>
#include <Renderer/Renderer.h>
#include <Renderer/VulkanService.h>
#include <State/LibraryManager.h>
#include <State/SceneManager.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>



class App;
inline std::unordered_map<GLFWwindow*, App*> g_windowMap;


class App final :
	public IUiPresenterDelegate,
	public ILibraryManagerDelegate,
	public ISceneManagerDelegate,
	public IVulkanServiceDelegate
{
public: // DATA
	bool FramebufferResized = false;

	
private: // DATA

	// Dependencies
	std::unique_ptr<IModelLoaderService> _modelLoaderService = nullptr;
	std::unique_ptr<VulkanService> _vulkanService = nullptr;
	std::unique_ptr<SceneManager> _scene = nullptr;
	std::unique_ptr<LibraryManager> _library = nullptr;;
	std::unique_ptr<UiPresenter> _ui = nullptr;
	std::unique_ptr<Renderer> _renderer = nullptr;

	// Window
	glm::ivec2 _windowSize = { 1600,900 };
	GLFWwindow* _window = nullptr;
	AppOptions _appOptions;
	bool _firstCursorInput = true;
	double _lastCursorX{}, _lastCursorY{};

	bool _defaultSceneLoaded = false;
	bool _updateEntities = true;
	
	// Time
	std::chrono::steady_clock::time_point _startTime = std::chrono::high_resolution_clock::now();
	std::chrono::steady_clock::time_point _lastFrameTime = std::chrono::high_resolution_clock::now();
	float _totalTime = 0;

	std::chrono::steady_clock::time_point _lastFpsUpdate;
	const std::chrono::duration<float, std::chrono::seconds::period> _reportFpsRate{ 1 };
	FpsCounter _fpsCounter{};

	// ImGui
	VkDescriptorPool _imguiDescriptorPool = nullptr;

	// State
	std::vector<int> _deletionQueue{};

	
public: // METHODS

	// Lifetime
	explicit App(AppOptions options)
	{
		InitWindow(options);

		// Services
		auto modelLoaderService = std::make_unique<AssimpModelLoaderService>();
		auto vulkan = std::make_unique<VulkanService>(options.EnabledVulkanValidationLayers, options.VSync, this);
	
		// Controllers
		auto scene = std::make_unique<SceneManager>(*this, *modelLoaderService);
		auto library = std::make_unique<LibraryManager>(*this, *scene, *modelLoaderService, options.AssetsDir);

		// UI
		auto renderer = std::make_unique<Renderer>(vulkan.get(), options.ShaderDir, options.AssetsDir, *modelLoaderService);
		auto ui = std::make_unique<UiPresenter>(*this, *library, *scene, *renderer, *vulkan, options.ShaderDir);

		// Set all teh things
		_appOptions = std::move(options);
		_modelLoaderService = std::move(modelLoaderService);
		_renderer = std::move(renderer);
		_scene = std::move(scene);
		_ui = std::move(ui);
		_library = std::move(library);
		_vulkanService = std::move(vulkan);

	}
	App(const App& other) = delete;
	App(App&& other) = delete;
	App& operator=(const App& other) = delete;
	App& operator=(App&& other) = delete;
	~App()
	{
		_renderer->CleanUp(); // TODO Fix this: Need to do first as renderer cleanup waits for device to idle.
		_ui->Shutdown();
		DestroyImgui();
		_vulkanService->Shutdown();
		
		glfwDestroyWindow(_window);
		glfwTerminate();
	}
	
	void Run()
	{
		// Init the things
		InitImgui();
		_library->LoadEmptyScene();

		
		// Update UI only as quickly as the monitor's refresh rate
		const auto* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		if (videoMode) {
			_ui->SetUpdateRate(videoMode->refreshRate);
		} else {
			std::cerr << "Failed to set vsync\n";
		}
		
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();

			// Compute time elapsed
			const auto currentTime = std::chrono::high_resolution_clock::now();
			_totalTime = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - _startTime).count();
			const auto dt = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - _lastFrameTime).count();
			_lastFrameTime = currentTime;


			// Report fps
			_fpsCounter.AddFrameTime(dt);
			if ((currentTime - _lastFpsUpdate) > _reportFpsRate)
			{
				char buffer[32];
				snprintf(buffer, 32, "Flux - %.1f fps", _fpsCounter.GetFps());
				glfwSetWindowTitle(_window, buffer);
				_lastFpsUpdate = currentTime;
			}

			Update(dt);
			Draw(dt);

			// Loading after first frame drawn to make app load feel more responsive - TEMP This is just while the demo scene default loads
			if (!_defaultSceneLoaded)
			{
				if (_appOptions.LoadDemoScene) {
					_library->LoadDemoScene();
				} else {
					_library->LoadDefaultScene();
				}

				_ui->FrameSelectionOrAll();
				
				_defaultSceneLoaded = true;
			}
		}
	}

private: // METHODS
	void InitWindow(const AppOptions& options)
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't use opengl

		GLFWwindow* window = glfwCreateWindow(_windowSize.x, _windowSize.y, "Vulkan", nullptr, nullptr);
		if (window == nullptr)
		{
			throw std::runtime_error("Failed to create GLFWwindow");
		}
		_window = window;
		g_windowMap.insert(std::make_pair(_window, this));

		//glfwSetWindowUserPointer(_window, this);
		
		glfwSetWindowSizeCallback(_window, WindowSizeCallback);
		glfwSetFramebufferSizeCallback(_window, FramebufferSizeCallback);
		glfwSetKeyCallback(_window, KeyCallback);
		glfwSetCursorPosCallback(_window, CursorPosCallback);
		glfwSetScrollCallback(_window, ScrollCallback);


		// Load icon
		{
			const auto iconPath = options.AssetsDir + "icon_32.png";

			int outChannelsInFile;
			GLFWimage icon;
			icon.pixels = stbi_load(iconPath.c_str(), &icon.width, &icon.height, &outChannelsInFile, 4);
			if (!icon.pixels)
			{
				stbi_image_free(icon.pixels);
				throw std::runtime_error("Failed to load texture image: " + iconPath);
			}
			
			glfwSetWindowIcon(window, 1, &icon);
			
			stbi_image_free(icon.pixels);
		}
	}

	void ProcessDeletionQueue()
	{
		for (auto& entId : _deletionQueue)
		{
			_ui->ClearSelection();
			_scene->RemoveEntity(entId);
		}

		_deletionQueue.clear();
	}

	void Update(const float dt)
	{
		ProcessDeletionQueue();

		if (_updateEntities) 
		{
			for (const auto& entity : _scene->EntitiesView()) 
			{
				if (entity->Action) 
				{
					entity->Action->Update(dt);
				}
			}
		}
		
	}

	void Draw(const float dt) const
	{
		const auto frameInfo = _vulkanService->StartFrame();
		if (!frameInfo.has_value())
		{
			return;
		}

		u32 imageIndex;
		VkCommandBuffer cmdBuf;
		std::tie(imageIndex, cmdBuf) = *frameInfo;
		
		_ui->Draw(imageIndex, cmdBuf);
		
		_vulkanService->EndFrame(imageIndex, cmdBuf);
	}

	// TODO Move to utils class
	static float RandF(float min, float max)
	{
		const auto base = float(rand()) / RAND_MAX;
		return min + base * (max - min);
	}


	#pragma region ImGui

	void InitImgui()
	{
		auto& _vk = _vulkanService;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();


		// UI Style
		{
			// Colors
			{
				ImGui::StyleColorsDark();
				auto& colors = ImGui::GetStyle().Colors;
				colors[ImGuiCol_WindowBg]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
				colors[ImGuiCol_Text]                   = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

				colors[ImGuiCol_Button]                 = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);

				colors[ImGuiCol_Border]                 = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
				colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
				colors[ImGuiCol_Separator]              = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

				colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 0.75f);
				colors[ImGuiCol_Header]                 = ImVec4(0.24f, 0.52f, 0.88f, 0.75f);

				colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.24f, 0.52f, 0.88f, 0.25f);
				colors[ImGuiCol_FrameBgActive]          = ImVec4(0.24f, 0.52f, 0.88f, 0.50f);
			}
			
			// Form
			{
				const float rounding = 3;
				ImGuiStyle& style = ImGui::GetStyle();
				//Main
				style.WindowPadding = {4,4};
				style.FramePadding = {6,3};
				style.ItemSpacing = {4,3};
				style.ItemInnerSpacing = {4,4};
				style.IndentSpacing = 21;
				style.ScrollbarSize = 9;
				style.GrabMinSize = 10;
				//Borders
				style.WindowBorderSize = 0;
				style.ChildBorderSize = 1;
				style.PopupBorderSize = 1;
				style.FrameBorderSize = 1;
				style.TabBorderSize = 0;
				//Rounding
				style.WindowRounding = 0;
				style.ChildRounding = rounding;
				style.FrameRounding = rounding;
				style.GrabRounding = 2;
			}
		}

		// Here Imgui is coupled to Glfw and Vulkan
		ImGui_ImplGlfw_InitForVulkan(_window, true);



		const auto imageCount = _vk->SwapchainImageCount();

		// Create descriptor pool - from main_vulkan.cpp imgui example code
		_imguiDescriptorPool = vkh::CreateDescriptorPool({
			 { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			 { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			 { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			 { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			}, imageCount, _vk->LogicalDevice());


		// Get the min image count
		u32 minImageCount;
		{
			// Copied from VulkanHelpers::CreateSwapchain - TODO either store minImageCount or make it a separate func
			const SwapChainSupportDetails deets = vkh::QuerySwapChainSupport(_vk->PhysicalDevice(), _vk->Surface());

			minImageCount = deets.Capabilities.minImageCount + 1; // 1 extra image to avoid waiting on driver
			const auto maxImageCount = deets.Capabilities.maxImageCount;
			const auto maxImageCountExists = maxImageCount != 0;
			if (maxImageCountExists && minImageCount > maxImageCount)
			{
				minImageCount = maxImageCount;
			}
		}


		// Init device info
		ImGui_ImplVulkan_InitInfo initInfo = {};
		{
			initInfo.Instance = _vk->Instance();
			initInfo.PhysicalDevice = _vk->PhysicalDevice();
			initInfo.Device = _vk->LogicalDevice();
			initInfo.QueueFamily = vkh::FindQueueFamilies(_vk->PhysicalDevice(), _vk->Surface()).GraphicsFamily.value(); // vomit
			initInfo.Queue = _vk->GraphicsQueue();
			initInfo.PipelineCache = nullptr;
			initInfo.DescriptorPool = _imguiDescriptorPool;
			initInfo.MinImageCount = minImageCount;
			initInfo.ImageCount = imageCount;
			initInfo.MSAASamples = _vk->MsaaSamples();
			initInfo.Allocator = nullptr;
			initInfo.CheckVkResultFn = [](VkResult err)
			{
				if (err == VK_SUCCESS) return;
				printf("VkResult %d\n", err);
				if (err < 0)
					abort();
			};
		}

		ImGui_ImplVulkan_Init(&initInfo, _vk->SwapchainRenderPass());


		// Upload Fonts
		{
			auto* const cmdBuf = vkh::BeginSingleTimeCommands(_vk->CommandPool(), _vk->LogicalDevice());
			ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
			vkh::EndSingeTimeCommands(cmdBuf, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->LogicalDevice());
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
	}
	void DestroyImgui() const
	{
		vkDestroyDescriptorPool(_vulkanService->LogicalDevice(), _imguiDescriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	#pragma endregion


	
	#pragma region IUiPresenterDelegate

	glm::ivec2 GetWindowSize() const override
	{
		return _windowSize;
	}
	void Delete(const std::vector<int>& entityIds) override
	{
		for (auto id : entityIds)
		{
			_deletionQueue.push_back(id);
		}
	}

	#pragma endregion

	

	#pragma region IVulkanServiceDelegate

	void NotifySwapchainUpdated(u32 width, u32 height, u32 numSwapchainImages) override
	{
		_renderer->HandleSwapchainRecreated(width, height, numSwapchainImages);
	}
	VkSurfaceKHR CreateSurface(VkInstance instance) const override
	{
		VkSurfaceKHR surface;
		if (glfwCreateWindowSurface(instance, _window, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}
		return surface;
	}
	VkExtent2D GetFramebufferSize() override
	{
		i32 width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		return { (u32)width, (u32)height };
	}
	VkExtent2D WaitTillFramebufferHasSize() override
	{
		// This handles a minimized window. Wait until it has size > 0
		i32 width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(_window, &width, &height);
			glfwWaitEvents();
		}

		return { (u32)width, (u32)height };
	}

	#pragma endregion

	
	
	#pragma region GLFW Callbacks, event handling

	// Callbacks
	static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
	{
		g_windowMap[window]->OnScrollChanged(xOffset, yOffset);
	}
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		g_windowMap[window]->OnKeyCallback(key, scancode, action, mods);
	}
	static void CursorPosCallback(GLFWwindow* window, double xPos, double yPos)
	{
		g_windowMap[window]->OnCursorPosChanged(xPos, yPos);
	}
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		//printf("TODO Handle FramebufferSizeCallback()\n");
	}
	static void WindowSizeCallback(GLFWwindow* window, int width, int height)
	{
		g_windowMap[window]->OnWindowSizeChanged(width, height);
	}

	// Event handlers
	void OnScrollChanged(double xOffset, double yOffset)
	{
		// TODO Refactor - this is ugly as it's accessing the gui's state in a global way.
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
			return;

		
		_scene->GetCamera().ProcessMouseScroll(float(yOffset));
	}
	void OnKeyCallback(int key, int scancode, int action, int mods)
	{
		// TODO Refactor - this is ugly as it's accessing the gui's state in a global way.
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput || io.WantCaptureKeyboard)
			return;

		
		// ONLY on pressed is handled
		if (action == GLFW_REPEAT || action == GLFW_RELEASE) return;

		//if (key == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose(_window, 1); }
		if (key == GLFW_KEY_F)      { _ui->FrameSelectionOrAll(); }
		if (key == GLFW_KEY_C)      { _ui->NextSkybox(); }
		if (key == GLFW_KEY_N)      { _updateEntities = !_updateEntities; }
		if (key == GLFW_KEY_DELETE) { _ui->DeleteSelected(); }
	}
	void OnCursorPosChanged(double xPos, double yPos)
	{
		// TODO Refactor - this is ugly as it's accessing the gui's state in a global way.
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
			return;
		
		
		// On first input lets remove a snap
		if (_firstCursorInput)
		{
			_lastCursorX = xPos;
			_lastCursorY = yPos;
			_firstCursorInput = false;
		}

		const auto xDiff = xPos - _lastCursorX;
		const auto yDiff = _lastCursorY - yPos;
		_lastCursorX = xPos;
		_lastCursorY = yPos;



		const auto windowSize = GetFramebufferSize();
		const glm::vec2 diffRatio{ xDiff / windowSize.height, yDiff / windowSize.height };
		auto* const window = _window;
		const auto isLmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1);
		const auto isMmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_3);
		const auto isRmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2);

		
		// Camera control
		auto& camera = _scene->GetCamera();
		if (isLmb)
		{
			const float arcSpeed = 1.5f*3.1415f;
			camera.Arc(diffRatio.x * arcSpeed, diffRatio.y * arcSpeed);
		}
		if (isMmb || isRmb)
		{
			const auto dir = isMmb
				? glm::vec3{ diffRatio.x, -diffRatio.y, 0 } // mmb pan
			: glm::vec3{ 0, 0, diffRatio.y };     // rmb zoom

			const auto len = glm::length(dir);
			if (len > 0.000001f) // small float
			{
				auto speed = Speed::Normal;
				if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
				{
					speed = Speed::Slow;
				}
				if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
				{
					speed = Speed::Fast;
				}
				camera.Move(len, glm::normalize(dir), speed);
			}
		}
	}
	void OnWindowSizeChanged(int width, int height)
	{
		FramebufferResized = true;
		_windowSize.x = width;
		_windowSize.y = height;
	}

	#pragma endregion 


	
	#pragma region ILibraryManagerDelegate

	RenderableResourceId CreateRenderable(const MeshResourceId& meshId, const Material& material) override
	{
		return _renderer->CreateRenderable(meshId, material);
	}
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition) override
	{
		return _renderer->CreateMeshResource(meshDefinition);
	}

	#pragma endregion


	
	#pragma region ISceneManagerDelegate
	
	TextureResourceId CreateTextureResource(const std::string& path) override
	{
		return _renderer->CreateTextureResource(path);
	}
	const Material& GetMaterial(const RenderableResourceId& id) override
	{
		return _renderer->GetMaterial(id);
	}
	void SetMaterial(const RenderableResourceId& id, const Material& newMat) override
	{
		return _renderer->SetMaterial(id, newMat);
	}
	IblTextureResourceIds CreateIblTextureResources(const std::string& path) override
	{
		return _renderer->CreateIblTextureResources(path);
	}
	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo) override
	{
		return _renderer->CreateSkybox(createInfo);
	}
	void SetSkybox(const SkyboxResourceId& resourceId) override
	{
		return _renderer->SetSkybox(resourceId);
	}
	
	#pragma endregion 
};
