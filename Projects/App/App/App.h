#pragma once

#include "GlfwWindow.h"
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
#include <utility>
#include <vector>



class App final :
	public IUiPresenterDelegate,
	public ILibraryManagerDelegate,
	public ISceneManagerDelegate,
	public IVulkanServiceDelegate
{
public: // DATA
	
private: // DATA
	
	// Dependencies
	std::unique_ptr<IModelLoaderService> _modelLoaderService = nullptr;
	std::unique_ptr<VulkanService> _vulkanService = nullptr;
	std::unique_ptr<SceneManager> _scene = nullptr;
	std::unique_ptr<LibraryManager> _library = nullptr;;
	std::unique_ptr<UiPresenter> _ui = nullptr;
	std::unique_ptr<Renderer> _renderer = nullptr;
	std::unique_ptr<IWindow> _window = nullptr;

	// Window
	AppOptions _appOptions;

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
		auto window = std::make_unique<GlfwWindow>();
		window->InitWindow();
		window->SetIcon(options.AssetsDir + "icon_32.png");
		
		// Services
		auto modelLoaderService = std::make_unique<AssimpModelLoaderService>();

		const auto builder = std::make_unique<GlfwVkSurfaceBuilder>(window->GetGlfwWindow());
		const auto size = window->GetFramebufferSize();
		const auto framebufferSize = VkExtent2D{size.Width, size.Height};
		auto vulkanService = std::make_unique<VulkanService>(options.EnabledVulkanValidationLayers, options.VSync, this, 
			builder.get(), framebufferSize);
	
		// Controllers
		auto scene = std::make_unique<SceneManager>(*this, *modelLoaderService);
		auto library = std::make_unique<LibraryManager>(*this, *scene, *modelLoaderService, options.AssetsDir);

		// UI
		auto renderer = std::make_unique<Renderer>(vulkanService.get(), options.ShaderDir, options.AssetsDir, *modelLoaderService);
		auto ui = std::make_unique<UiPresenter>(*this, *library, *scene, *renderer, *vulkanService, window.get(), options.ShaderDir);

		InitImgui(window->GetGlfwWindow(), *vulkanService);
		
		// Set all teh things
		_appOptions = std::move(options);
		_modelLoaderService = std::move(modelLoaderService);
		_renderer = std::move(renderer);
		_scene = std::move(scene);
		_ui = std::move(ui);
		_library = std::move(library);
		_vulkanService = std::move(vulkanService);
		_window = std::move(window);
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
	}
	
	void Run()
	{
		// Init the things
		_library->LoadEmptyScene();

		
		// Update UI only as quickly as the monitor's refresh rate
		const auto* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		if (videoMode) {
			_ui->SetUpdateRate(videoMode->refreshRate);
		} else {
			std::cerr << "Failed to set vsync\n";
		}
		
		while (!_window->CloseRequested())
		{
			_window->PollEvents();

			// Compute time elapsed
			const auto currentTime = std::chrono::high_resolution_clock::now();
			_totalTime = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - _startTime).count();
			const auto dt = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - _lastFrameTime).count();
			_lastFrameTime = currentTime;


			// Report fps
			_fpsCounter.AddFrameTime(dt);
			if ((currentTime - _lastFpsUpdate) > _reportFpsRate)
			{
				char title[32];
				snprintf(title, 32, "Flux - %.1f fps", _fpsCounter.GetFps());
				_window->SetWindowTitle(title);
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

	void InitImgui(GLFWwindow* window, VulkanService& vk)
	{
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
		ImGui_ImplGlfw_InitForVulkan(window, true);



		const auto imageCount = vk.SwapchainImageCount();

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
			}, imageCount, vk.LogicalDevice());


		// Get the min image count
		u32 minImageCount;
		{
			// Copied from VulkanHelpers::CreateSwapchain - TODO either store minImageCount or make it a separate func
			const SwapChainSupportDetails deets = vkh::QuerySwapChainSupport(vk.PhysicalDevice(), vk.Surface());

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
			initInfo.Instance = vk.Instance();
			initInfo.PhysicalDevice = vk.PhysicalDevice();
			initInfo.Device = vk.LogicalDevice();
			initInfo.QueueFamily = vkh::FindQueueFamilies(vk.PhysicalDevice(), vk.Surface()).GraphicsFamily.value(); // vomit
			initInfo.Queue = vk.GraphicsQueue();
			initInfo.PipelineCache = nullptr;
			initInfo.DescriptorPool = _imguiDescriptorPool;
			initInfo.MinImageCount = minImageCount;
			initInfo.ImageCount = imageCount;
			initInfo.MSAASamples = vk.MsaaSamples();
			initInfo.Allocator = nullptr;
			initInfo.CheckVkResultFn = [](VkResult err)
			{
				if (err == VK_SUCCESS) return;
				printf("VkResult %d\n", err);
				if (err < 0)
					abort();
			};
		}

		ImGui_ImplVulkan_Init(&initInfo, vk.SwapchainRenderPass());


		// Upload Fonts
		{
			auto* const cmdBuf = vkh::BeginSingleTimeCommands(vk.CommandPool(), vk.LogicalDevice());
			ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
			vkh::EndSingeTimeCommands(cmdBuf, vk.CommandPool(), vk.GraphicsQueue(), vk.LogicalDevice());
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

	void Delete(const std::vector<int>& entityIds) override
	{
		for (auto id : entityIds)
		{
			_deletionQueue.push_back(id);
		}
	}

	void ToggleUpdateEntities() override
	{
		_updateEntities = !_updateEntities;
	}

	#pragma endregion

	

	#pragma region IVulkanServiceDelegate

	void NotifySwapchainUpdated(u32 width, u32 height, u32 numSwapchainImages) override
	{
		_renderer->HandleSwapchainRecreated(width, height, numSwapchainImages);
	}
	
	VkExtent2D GetFramebufferSize() override
	{
		const auto size = _window->GetFramebufferSize();
		return VkExtent2D{size.Width, size.Height};
	}
	VkExtent2D WaitTillFramebufferHasSize() override
	{
		const auto size = _window->WaitTillFramebufferHasSize();
		return VkExtent2D{size.Width, size.Height};
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
