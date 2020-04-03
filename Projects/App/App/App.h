#pragma once

#include "AppTypes.h"
#include "AssImpModelLoaderService.h"
#include "FpsCounter.h"
#include "UI/UiPresenter.h"

#include <Renderer/CubemapTextureLoader.h>
#include <Renderer/Renderer.h>
#include <Renderer/VulkanService.h>
#include <State/Entity/Actions/TurntableActionComponent.h>
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


// TODO Extract IWindow interface and VulkanWindow impl from App
class App final :
	public IRendererDelegate,
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

	explicit App(AppOptions options)
	{
		InitWindow();

		// Services
		auto modelLoaderService = std::make_unique<AssimpModelLoaderService>();
		auto vulkan = std::make_unique<VulkanService>(options.EnabledVulkanValidationLayers, this);

		// Controllers
		auto scene = std::make_unique<SceneManager>(this);
		auto library = std::make_unique<LibraryManager>(this, modelLoaderService.get(), options.AssetsDir);

		// UI
		auto renderer = std::make_unique<Renderer>(vulkan.get(), options.ShaderDir, options.AssetsDir, *this, *modelLoaderService);
		auto ui = std::make_unique<UiPresenter>(*this, *library, *scene, *renderer);

		// Set all teh things
		_appOptions = std::move(options);
		_modelLoaderService = std::move(modelLoaderService);
		_renderer = std::move(renderer);
		_scene = std::move(scene);
		_ui = std::move(ui);
		_library = std::move(library);
		_vulkanService = std::move(vulkan);

	}
	
	~App()
	{
		_renderer->CleanUp();
		DestroyImgui();
		_vulkanService->DestroyVulkanSwapchain();
		_vulkanService->DestroyVulkan();

		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	void Run()
	{
		InitImgui();
		LoadDefaultScene();

		// Update UI only as quickly as the monitor's refresh rate
		//auto vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		//_uiUpdateRate = std::chrono::duration<float, std::chrono::seconds::period>(1.f / (f32)vidMode->refreshRate);

		
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
		}
	}


private: // METHODS
	void InitWindow()
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

		for (auto& entity : _scene->EntitiesView())
		{
			if (entity->Action)
			{
				entity->Action->Update(dt);
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
		const float rounding = 3;

		ImGui::StyleColorsLight();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 0;
		style.WindowBorderSize = 0;
		style.WindowRounding = 0;
		style.FrameRounding = rounding;
		style.ChildRounding = rounding;



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

		ImGui_ImplVulkan_Init(&initInfo, _vk->RenderPass());


		// Upload Fonts
		{
			const auto cmdBuf = vkh::BeginSingleTimeCommands(_vk->CommandPool(), _vk->LogicalDevice());
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

	// TODO Move to _ui layer to make a single spot where UI drives state
	void LoadDefaultScene()
	{
		_ui->LoadSkybox(_library->GetSkyboxes()[0].Path);
		LoadMaterialArray();

		/*
		auto entity = _library->CreateBlob();
		Material mat = {};
		mat.Basecolor = glm::vec3{ 1 };
		mat.Metalness = 1;
		mat.Roughness = 0;
		_scene->SetMaterial(entity->Renderable->RenderableId, mat);
		_scene->AddEntity(std::move(entity));
		*/

		/*	auto blob = _library->CreateBlob();
			blob->Action = std::make_unique<TurntableAction>(blob->Transform);
			_scene->AddEntity(std::move(blob));*/

		_ui->FrameSelectionOrAll();
	}

	// TODO Move to _ui layer to make a single spot where UI drives state
	void LoadDemoScene() override
	{
		std::cout << "Loading scene\n";
		_ui->LoadSkybox(_library->GetSkyboxes()[0].Path);
		LoadAxis();
		LoadMaterialArray({ 0,4,0 });
		LoadRailgun();
		//LoadLighting();
	}

	void LoadDemoSceneHeavy() override
	{
		std::cout << "Loading scene\n";
		_ui->LoadSkybox(_library->GetSkyboxes()[0].Path);
		LoadAxis();
		LoadMaterialArray({ 0,0,0 }, 30, 30);
	}

	#pragma endregion

	

	#pragma region IRendererDelegate

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

	
	
	#pragma region Scene Management // TODO move out of App.h

	void LoadMaterialArray(const glm::vec3& offset = glm::vec3{ 0,0,0 }, u32 numRows = 2, u32 numColumns = 5)
	{
		std::cout << "Loading material array" << std::endl;

		glm::vec3 center = offset;
		auto rowSpacing = 2.1f;
		auto colSpacing = 2.1f;

		u32 count = 0;
		
		for (u32 row = 0; row < numRows; row++)
		{
			f32 metalness = row / f32(numRows - 1);

			f32 height = f32(numRows - 1) * rowSpacing;
			f32 hStart = height / 2.f;
			f32 y = center.y + hStart + -rowSpacing * row;

			for (u32 col = 0; col < numColumns; col++)
			{
				f32 roughness = col / f32(numColumns - 1);

				f32 width = f32(numColumns - 1) * colSpacing;
				f32 wStart = -width / 2.f;
				f32 x = center.x + wStart + colSpacing * col;

				char name[256];
				sprintf_s(name, 256, "Obj M:%.2f R:%.2f", metalness, roughness);
				
				auto entity = _library->CreateBlob();
				entity->Name = name;
				entity->Transform.SetPos({ x,y,0.f });
				entity->Action = std::make_unique<TurntableAction>(entity->Transform);
				
				// config mat
				Material mat = {};
				mat.Basecolor = glm::vec3{ 1 };
				mat.Roughness = roughness;
				mat.Metalness = metalness;

				_scene->SetMaterial(*entity->Renderable, mat);

				_scene->AddEntity(std::move(entity));

				++count;
			}
		}

		std::cout << "Material array obj count: " << count << std::endl; 
	}
	
	void LoadRailgun()
	{
		const auto path = _appOptions.ModelsDir + "railgun/q2railgun.gltf";
		std::cout << "Loading model:" << path << std::endl;


		auto entity = std::make_unique<Entity>();
		entity->Name = "Railgun";
		entity->Transform.SetPos(glm::vec3{ 0, -3, 0 });
		entity->Renderable = _scene->LoadRenderableComponentFromFile(path);
		entity->Action = std::make_unique<TurntableAction>(entity->Transform);


		// Add more maps to the material
		{
			const RenderableResourceId resourceId = entity->Renderable->GetSubmeshes()[0].Id;
			
			Material matCopy = _scene->GetMaterial(resourceId);

			// Load roughness map
			matCopy.RoughnessMap = _scene->LoadTexture(_appOptions.ModelsDir + "railgun/ORM.png");
			matCopy.UseRoughnessMap = true;
			matCopy.RoughnessMapChannel = Material::Channel::Green;

			// Load metalness map
			matCopy.MetalnessMap = _scene->LoadTexture(_appOptions.ModelsDir + "railgun/ORM.png");
			matCopy.UseMetalnessMap = true;
			matCopy.MetalnessMapChannel = Material::Channel::Blue;

			// Set material
			_scene->SetMaterial(resourceId, matCopy);
		}


		_scene->AddEntity(std::move(entity));
	}

	void LoadAxis()
	{
		std::cout << "Loading axis" << std::endl;

		auto scale = glm::vec3{ 0.5f };
		f32 dist = 1;

		// Pivot
		{
			auto entity = _library->CreateSphere();
			entity->Name = "Axis-Pivot";
			entity->Transform.SetScale(scale*0.5f);
			entity->Transform.SetPos(glm::vec3{ 0, 0, 0 });


			{
				Material mat;

				auto basePath = _appOptions.AssetsDir + "Materials/ScuffedAluminum/BaseColor.png";
				auto ormPath = _appOptions.AssetsDir + "Materials/ScuffedAluminum/ORM.png";
				auto normalPath = _appOptions.AssetsDir + "Materials/ScuffedAluminum/Normal.png";
				
				mat.UseBasecolorMap = true;
				mat.BasecolorMapPath = basePath;
				mat.BasecolorMap = _scene->LoadTexture(basePath);

				//mat.UseNormalMap = true;
				mat.NormalMapPath = normalPath;
				mat.NormalMap = _scene->LoadTexture(normalPath);
				
				//mat.UseAoMap = true;
				mat.AoMapPath = ormPath;
				mat.AoMap = _scene->LoadTexture(ormPath);
				mat.AoMapChannel = Material::Channel::Red;
				
				mat.UseRoughnessMap = true;
				mat.RoughnessMapPath = ormPath;
				mat.RoughnessMap = _scene->LoadTexture(ormPath);
				mat.RoughnessMapChannel = Material::Channel::Green;

				mat.UseMetalnessMap = true;
				mat.MetalnessMapPath = ormPath;
				mat.MetalnessMap = _scene->LoadTexture(ormPath);
				mat.MetalnessMapChannel = Material::Channel::Blue;

				_scene->SetMaterial(*entity->Renderable, mat);
			}

			_scene->AddEntity(std::move(entity));
		}
		
		// X
		{
			auto entity = _library->CreateSphere();
			entity->Name = "Axis-X";
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ dist, 0, 0 });

			Material mat{};
			{
				mat.Basecolor = { 1,0,0 };

				//mat.UseNormalMap = true;
				mat.NormalMap = _scene->LoadTexture(_appOptions.AssetsDir + "Materials/BumpyPlastic/Normal.png");

				//mat.UseAoMap = true;
				mat.AoMap = _scene->LoadTexture(_appOptions.AssetsDir + "Materials/BumpyPlastic/ORM.png");
				mat.AoMapChannel = Material::Channel::Red;

				mat.UseRoughnessMap = true;
				mat.RoughnessMap = _scene->LoadTexture(_appOptions.AssetsDir + "Materials/BumpyPlastic/ORM.png");
				mat.RoughnessMapChannel = Material::Channel::Green;

				mat.UseMetalnessMap = true;
				mat.MetalnessMap = _scene->LoadTexture(_appOptions.AssetsDir + "Materials/BumpyPlastic/ORM.png");
				mat.MetalnessMapChannel = Material::Channel::Blue;
			}
			_scene->SetMaterial(*entity->Renderable, mat);

			_scene->AddEntity(std::move(entity));
		}
		
		// Y
		{
			auto entity = _library->CreateSphere();
			entity->Name = "Axis-Y";
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ 0, dist, 0 });
			
			Material mat{};
			mat.Basecolor = { 0,1,0 };
			mat.Roughness = 0;
			_scene->SetMaterial(*entity->Renderable, mat);
			
			_scene->AddEntity(std::move(entity));
		}
		
		// Z
		{
			auto entity = _library->CreateSphere();
			entity->Name = "Axis-Z";
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ 0, 0, dist });
			
			Material mat{};
			mat.Basecolor = { 0,0,1 };
			mat.Roughness = 0;
			_scene->SetMaterial(*entity->Renderable, mat);
			
			_scene->AddEntity(std::move(entity));
		}
	}

	void LoadLighting()
	{
		auto RandF = [] (float min, float max)
		{
			const auto base = float(rand()) / RAND_MAX;
			return min + base * (max - min);
		};
		
		// Directional light
		auto dirLight = std::make_unique<Entity>();
		dirLight->Name = "PointLight";
		dirLight->Transform.SetPos({ -1, -1, -1});
		dirLight->Light = LightComponent{};
		dirLight->Light->Color = { 1,1,1 };
		dirLight->Light->Intensity = 20;
		dirLight->Light->Type = LightComponent::Types::directional;
		_scene->AddEntity(std::move(dirLight));

		// Max random lights
		for (int i = 0; i < 7; i++)
		{
			auto light = std::make_unique<Entity>();
			light->Name = "PointLight";
			light->Transform.SetPos({ RandF(-10,10),RandF(-10,10),RandF(-10,10) });
			light->Light = LightComponent{};
			light->Light->Color = { RandF(0,1),RandF(0,1),RandF(0,1) };
			light->Light->Intensity = 300;
			light->Light->Type = LightComponent::Types::point;
			_scene->AddEntity(std::move(light));
		}
	}

	void RandomizeLights()
	{
		for (auto& entity : _scene->EntitiesView())
		{
			if (entity->Light.has_value())
			{
				entity->Transform.SetPos({ RandF(-10,10),RandF(-10,10),RandF(-10,10) });
				entity->Light->Color = { RandF(0,1),RandF(0,1),RandF(0,1) };
			}
		}
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
		if (key == GLFW_KEY_L)      { RandomizeLights(); }
		if (key == GLFW_KEY_C)      { _ui->NextSkybox(); }
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
		const glm::vec2 diffRatio{ xDiff / windowSize.width, yDiff / windowSize.height };
		const auto window = _window;
		const auto isLmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1);
		const auto isMmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_3);
		const auto isRmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2);

		
		// Camera control
		auto& camera = _scene->GetCamera();
		if (isLmb)
		{
			const float arcSpeed = 3;
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


	
	#pragma region ILibraryManagerDelegate

	std::optional<ModelDefinition> LoadModel(const std::string& path) override
	{
		return _modelLoaderService->LoadModel(path);
	}
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
