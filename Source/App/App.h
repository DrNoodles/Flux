#pragma once

#include "AppTypes.h"
#include "Camera.h"
#include "Entity/Entity.h"
#include "FpsCounter.h"
#include "IModelLoaderService.h"
#include "SceneManager.h"
#include "AssImpModelLoaderService.h"
#include "UI/UiPresenter.h"

#include <Renderer/Renderer.h>
#include <Renderer/CubemapTextureLoader.h>

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
class App final : public IRendererDelegate, public IUiPresenterDelegate
{
public:

	bool FramebufferResized = false;

	explicit App(AppOptions options)
	{
		InitWindow();


		// Services
		auto modelLoaderService = std::make_unique<AssimpModelLoaderService>();


		// Controllers
		auto renderer = std::make_unique<Renderer>(options.EnabledVulkanValidationLayers, options.ShaderDir, options.AssetsDir, *this, *modelLoaderService);
		auto scene = std::make_unique<SceneManager>(*modelLoaderService, *renderer);
		auto library = std::make_unique<LibraryManager>(*renderer, modelLoaderService.get(), options.AssetsDir);

		// UI
		auto ui = std::make_unique<UiPresenter>(*this, library.get(), *scene/*dependencies*/);


		// Set all teh things
		_appOptions = std::move(options);
		_modelLoaderService = std::move(modelLoaderService);
		_renderer = std::move(renderer);
		_scene = std::move(scene);
		_ui = std::move(ui);
		_library = std::move(library);
	}
	~App()
	{
		_renderer->CleanUp();

		glfwDestroyWindow(_window);
		glfwTerminate();
	}


	void Run()
	{
		LoadDefaultScene();

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
		LoadSphereArray();

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
		LoadSphereArray({0,4,0});
		LoadRailgun();
		//LoadLighting();
	}

	RenderOptions& GetRenderOptions() override
	{
		return _renderOptions;
	}
	
	void SetRenderOptions(const RenderOptions& ro) override
	{
		_renderOptions = ro;
	}
	
	#pragma endregion

	
	#pragma region IRendererDelegate
	
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

	// TODO Remove this horrible code D: The connection of glfw, imgui and vulkan need to be done somewhere else - ie. a factory class
	void InitImguiWithGlfwVulkan() override
	{
		ImGui_ImplGlfw_InitForVulkan(_window, true);
	}
	
	void BuildGui() override
	{
		_ui->Draw();
	}

	#pragma endregion

	
private:
	// Dependencies
	std::unique_ptr<IModelLoaderService> _modelLoaderService = nullptr;
	std::unique_ptr<SceneManager> _scene = nullptr;
	std::unique_ptr<LibraryManager> _library = nullptr;;
	std::unique_ptr<UiPresenter> _ui = nullptr;
	std::unique_ptr<Renderer> _renderer = nullptr;
	
	glm::ivec2 _windowSize = { 1280,720 };
	GLFWwindow* _window = nullptr;
	AppOptions _appOptions;
	RenderOptions _renderOptions;
	

	// Time
	std::chrono::steady_clock::time_point _startTime = std::chrono::high_resolution_clock::now();
	std::chrono::steady_clock::time_point _lastFrameTime = std::chrono::high_resolution_clock::now();
	float _totalTime = 0;
	std::chrono::steady_clock::time_point _lastFpsUpdate;
	const std::chrono::duration<double, std::chrono::seconds::period> _reportFpsRate{ 1 };
	FpsCounter _fpsCounter{};


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
		glfwSetKeyCallback(_window, KeyCallback);
		glfwSetCursorPosCallback(_window, CursorPosCallback);
		glfwSetScrollCallback(_window, ScrollCallback);
	}

	
	std::vector<int> _deletionQueue{};
	void ProcessDeletionQueue()
	{
		for (auto& entId : _deletionQueue)
		{
			_ui->ClearSelection();
			_scene->RemoveEntity(entId);
		}

		_deletionQueue.clear();
	}

	
	// TODO Move to utils class
	static float RandF(float min, float max)
	{
		const auto base = float(rand()) / RAND_MAX;
		return min + base * (max - min);
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
		auto& entities = _scene->EntitiesView();
		std::vector<RenderableResourceId> renderables;
		std::vector<Light> lights;
		std::vector<glm::mat4> transforms;

		for (auto& entity : entities)
		{
			if (entity->Renderable.has_value())
			{
				for (auto&& componentSubmesh : entity->Renderable->GetSubmeshes())
				{
					renderables.emplace_back(componentSubmesh.Id);
					transforms.emplace_back(entity->Transform.GetMatrix());
				}
			}

			if (entity->Light.has_value())
			{
				auto light = entity->Light->ToLight();
				light.Pos = entity->Transform.GetPos();
				lights.emplace_back(light);
			}
		}


		auto& camera = _scene->GetCamera();
		const auto view = camera.GetViewMatrix();
		_renderer->DrawFrame(dt, _renderOptions, renderables, transforms, lights, view, camera.Position);
	}

	
	#pragma region Scene Management

	void LoadSphereArray(const glm::vec3& offset = glm::vec3{ 0,0,0 })
	{
		std::cout << "Loading sphere array" << std::endl;

		glm::vec3 center = offset;
		auto numRows = 2;
		auto numColumns = 6;
		auto rowSpacing = 2.1f;
		auto colSpacing = 2.1f;
		
		for (int row = 0; row < numRows; row++)
		{
			f32 metalness = row / f32(numRows - 1);

			f32 height = f32(numRows - 1) * rowSpacing;
			f32 hStart = height / 2.f;
			f32 y = center.y + hStart + -rowSpacing * row;

			for (int col = 0; col < numColumns; col++)
			{
				f32 roughness = col / f32(numColumns - 1);

				f32 width = f32(numColumns - 1) * colSpacing;
				f32 wStart = -width / 2.f;
				f32 x = center.x + wStart + colSpacing * col;

				char name[256];
				sprintf_s(name, 256, "Blob M:%.2f R:%.2f", metalness, roughness);
				
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
			}
		}
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


		//// Add more maps to the material
		//{
		//	Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);

		//	// Load roughness map
		//	matCopy.RoughnessMap = _scene->LoadTexture(_options.ModelsDir + "railgun/ORM.png");
		//	matCopy.UseRoughnessMap = true;
		//	matCopy.RoughnessMapChannel = Material::Channel::Green;

		//	// Load metalness map
		//	matCopy.MetalnessMap = _scene->LoadTexture(_options.ModelsDir + "railgun/ORM.png");
		//	matCopy.UseMetalnessMap = true;
		//	matCopy.MetalnessMapChannel = Material::Channel::Blue;

		//	// Set material
		//	_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);
		//}


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
	static void WindowSizeCallback(GLFWwindow* window, int width, int height)
	{
		g_windowMap[window]->OnWindowSizeChanged(width, height);
		
	}


	bool _firstCursorInput = true;
	double _lastCursorX{}, _lastCursorY{};
	
	// Event handlers
	void OnScrollChanged(double xOffset, double yOffset)
	{
		_scene->GetCamera().ProcessMouseScroll(float(yOffset));
	}
	void OnKeyCallback(int key, int scancode, int action, int mods)
	{
		// ONLY on pressed is handled
		if (action == GLFW_REPEAT || action == GLFW_RELEASE) return;

		if (key == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose(_window, 1); }
		if (key == GLFW_KEY_F)      { _ui->FrameSelectionOrAll(); }
		if (key == GLFW_KEY_L)      { RandomizeLights(); }
		if (key == GLFW_KEY_C)      { _ui->NextSkybox(); }
		if (key == GLFW_KEY_DELETE) { _ui->DeleteSelected(); }
	}
	void OnCursorPosChanged(double xPos, double yPos)
	{
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
};
