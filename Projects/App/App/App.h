#pragma once

#include "GlfwWindow.h"
#include "AppTypes.h"
#include "AssImpModelLoaderService.h"
#include "FpsCounter.h"
#include "UI/UiPresenter.h"
#include "ImGuiVulkanGlfw.h"

#include <Renderer/HighLevel/GraphicsPipelines/ForwardGraphicsPipeline.h> // HACK Remove this when the routing hacks below are gone.
#include <State/LibraryManager.h>
#include <State/SceneManager.h>

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
	std::unique_ptr<VulkanService>       _vulkanService      = nullptr;
	std::unique_ptr<SceneManager>        _scene              = nullptr;
	std::unique_ptr<LibraryManager>      _library            = nullptr;
	std::unique_ptr<UiPresenter>         _ui                 = nullptr;
	std::unique_ptr<IWindow>             _window             = nullptr;
	std::unique_ptr<ImGuiVulkanGlfw>     _imgui              = nullptr;

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


	// State
	std::vector<i32> _deletionQueue{}; // i32 is the EntityId to be deleted. Hack :)

	
public: // METHODS

	// Lifetime
	explicit App(AppOptions options)
	{
		// Services
		auto window = std::make_unique<GlfwWindow>();
		auto modelLoaderService = std::make_unique<AssimpModelLoaderService>();

		const auto builder = std::make_unique<GlfwVkSurfaceBuilder>(window->GetGlfwWindow());
		const auto size = window->GetFramebufferSize();
		const auto framebufferSize = VkExtent2D{size.Width, size.Height};
		auto vulkanService = std::make_unique<VulkanService>(options.EnabledVulkanValidationLayers, options.VSync, options.UseMsaa, this, builder.get(), framebufferSize);

		auto imgui = std::make_unique<ImGuiVulkanGlfw>(window->GetGlfwWindow(), vulkanService.get());
		
		// Controllers
		auto scene = std::make_unique<SceneManager>(*this, *modelLoaderService);
		auto library = std::make_unique<LibraryManager>(*this, *scene, *modelLoaderService, options.AssetsDir);

		// UI
		auto ui = std::make_unique<UiPresenter>(*this, *library, *scene, *vulkanService, window.get(), options.ShaderDir, options.AssetsDir, *modelLoaderService);

		
		// Set all teh things
		_appOptions = std::move(options);
		_modelLoaderService = std::move(modelLoaderService);
		_scene = std::move(scene);
		_ui = std::move(ui);
		_library = std::move(library);
		_vulkanService = std::move(vulkanService);
		_window = std::move(window);
		_imgui = std::move(imgui);

		Start();
	}
	App(const App& other) = delete;
	App(App&& other) = delete;
	App& operator=(const App& other) = delete;
	App& operator=(App&& other) = delete;
	~App() override
	{
		vkDeviceWaitIdle(_vulkanService->LogicalDevice());
		
		_ui = nullptr; // RAII
		_imgui = nullptr; // RAII
		_vulkanService = nullptr; // RAII
	}

	
private: // METHODS
	
	void Start()
	{
		// Init the things
		_window->SetIcon(_appOptions.AssetsDir + "icon_32.png");
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
			Draw();

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
	
	void Update(const float dt)
	{
		// ProcessDeletionQueue();
		{
			// TODO HACK: Move to unified command undo/redo queue in the State layer - when that exists...
			for (auto& entId : _deletionQueue)
			{
				_ui->ClearSelection();
				_scene->RemoveEntity(entId);
			}

			_deletionQueue.clear();
		}

		
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

		_ui->Update();
	}

	void Draw() const
	{
		const auto frameInfo = _vulkanService->StartFrame();
		if (!frameInfo.has_value())
		{
			return;
		}

		auto [imageIndex, cmdBuf] = *frameInfo;
		
		_ui->Draw(imageIndex, cmdBuf);
		
		_vulkanService->EndFrame(imageIndex, cmdBuf);
	}


	
	#pragma region IUiPresenterDelegate

	void Delete(const std::vector<i32>& entityIds) override
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
		_ui->HandleSwapchainRecreated(width, height, numSwapchainImages);
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

	RenderableResourceId CreateRenderable(const MeshResourceId& meshId) override
	{
		return _ui->HACK_GetSceneRendererRef().CreateRenderable(meshId);
	}
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition) override
	{
		return _ui->HACK_GetSceneRendererRef().Hack_CreateMeshResource(meshDefinition);
	}

	#pragma endregion


	
	#pragma region ISceneManagerDelegate
	
	TextureResourceId CreateTextureResource(const std::string& path) override
	{
		return _ui->HACK_GetSceneRendererRef().Hack_CreateTextureResource(path);
	}

	IblTextureResourceIds CreateIblTextureResources(const std::string& path) override
	{
		return _ui->HACK_GetSceneRendererRef().CreateIblTextureResources(path);
	}
	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo) override
	{
		return _ui->HACK_GetSceneRendererRef().CreateSkybox(createInfo);
	}
	void SetSkybox(const SkyboxResourceId& resourceId) override
	{
		_ui->HACK_GetSceneRendererRef().SetSkybox(resourceId);
	}

private:
	#pragma endregion 
};
