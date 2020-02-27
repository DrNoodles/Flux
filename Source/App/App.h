#pragma once

#include "AppTypes.h"
#include "Camera.h"
#include "Entity/Entity.h"
#include "FpsCounter.h"
#include "IModelLoaderService.h"
#include "SceneManager.h"
#include "AssImpModelLoaderService.h"
#include "UI/PropsView.h"
#include "UI/ScenePane.h"

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
class App final : public IRendererDelegate
{
public:

	bool FramebufferResized = false;

	
	explicit App(AppOptions options)
	{
		// Set Dependencies
		_options = std::move(options);
		
		// Init all the things
		InitWindow();

		// Services
		_modelLoaderService = std::make_unique<AssimpModelLoaderService>();

		_renderer = std::make_unique<Renderer>(_options.EnabledVulkanValidationLayers, _options.ShaderDir,
		                                       _options.AssetsDir, *this, *_modelLoaderService);
		_scene = std::make_unique<SceneManager>(*_modelLoaderService, *_renderer);
	}
	~App()
	{
		_renderer->CleanUp();

		glfwDestroyWindow(_window);
		glfwTerminate();
	}


	void Run()
	{
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();


			// Compute time elapsed
			const auto currentTime = std::chrono::high_resolution_clock::now();
			_totalTime = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - _startTime).count();
			const auto dt = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - _lastTime).count();
			_lastTime = currentTime;


			// Report fps
			_fpsCounter.AddFrameTime(dt);
			if ((currentTime - _lastFpsUpdate) > _reportFpsRate)
			{
				char buffer[32];
				snprintf(buffer, 32, "%.1f fps", _fpsCounter.GetFps());
				glfwSetWindowTitle(_window, buffer);
				_lastFpsUpdate = currentTime;
			}

			Update(dt);
			Draw(dt);
		}
	}


	void Update(const float dt) const
	{
		const auto degreesPerSec = 30.f;
		const auto rotationDelta = dt * degreesPerSec;

		for (auto& entity : _scene->GetEntities())
		{
			if (entity->Action)
			{
				entity->Action->Update(dt);
			}

			// TODO Temp rotation of all renderables. Convert  to turntable actions
			if (entity->Renderable.has_value())
			{
				auto& transform = entity->Transform;
				auto rot = transform.GetRot();
				rot.y += rotationDelta;
				transform.SetRot(rot);
			}
		}
	}

	
	void Draw(const float dt) const
	{
		auto& entities = _scene->GetEntities();
		std::vector<RenderableResourceId> renderables;
		std::vector<Light> lights;
		std::vector<glm::mat4> transforms;
		
		for (auto& entity : entities)
		{
			if (entity->Renderable.has_value())
			{
				renderables.emplace_back(entity->Renderable->RenderableId);
				transforms.emplace_back(entity->Transform.GetMatrix());
			}

			if (entity->Light.has_value())
			{
				auto light = entity->Light->ToLight();
				light.Pos = entity->Transform.GetPos();
				lights.emplace_back(light);
			}
		}


		auto& camera = _scene->GetCamera();
		auto view = camera.GetViewMatrix();
		_renderer->DrawFrame(dt, renderables, transforms, lights, view, camera.Position);
	}


	#pragma region IVulkanGpuServiceDelegate
	
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
	//Camera UpdateState(float dt) override
	//{
	//	return _camera;
	//}
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
	//void DrawUI(VkCommandBuffer commandBuffer)
	//{
	//	// Start the Dear ImGui frame
	//	ImGui_ImplVulkan_NewFrame();
	//	ImGui_ImplGlfw_NewFrame();
	//	ImGui::NewFrame();
	//	{
			/*
			glViewport(0, 0, _windowWidth, _windowWidth);

			// Scene Pane

			ImGui::SetNextWindowPos(ImVec2(float(ScenePos().x), float(ScenePos().y)));
			ImGui::SetNextWindowSize(ImVec2(float(SceneSize().x), float(SceneSize().y)));

			auto& entsView = _sceneController.EntitiesView();
			std::vector<Entity*> allEnts{ entsView.size() };
			std::transform(entsView.begin(), entsView.end(), allEnts.begin(), [](const std::unique_ptr<Entity>& pe)
				{
					return pe.get();
				});
			IblVm iblVm{ &_sceneController, &_renderOptions };
			_scenePane.DrawUI(allEnts, _selection, iblVm);


			// Properties Pane
			ImGui::SetNextWindowPos(ImVec2(float(PropsPos().x), float(PropsPos().y)));
			ImGui::SetNextWindowSize(ImVec2(float(PropsSize().x), float(PropsSize().y)));

			Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
			_propsPresenter.Draw((int)_selection.size(), selection, _textures, _meshes, _gpuResourceService.get());
			*/
	/*	}
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}*/

	#pragma endregion

	
private:
	// Dependencies
	std::unique_ptr<Renderer> _renderer = nullptr;
	std::unique_ptr<SceneManager> _scene = nullptr;
	std::unique_ptr<IModelLoaderService> _modelLoaderService = nullptr;
	
	const glm::ivec2 _defaultWindowSize = { 800,600 };
	GLFWwindow* _window = nullptr;
	AppOptions _options;

	// Skybox management
	u32 _currentSkybox = 0;
	std::unordered_map<std::string, SkyboxResourceId> _loadedSkyboxes = {};
	std::vector<std::string> _skyboxPaths =
	{
		"ChiricahuaPath.hdr",
		"DitchRiver.hdr",
		"debug/equirectangular.hdr",
	};


	// Time
	std::chrono::steady_clock::time_point _startTime = std::chrono::high_resolution_clock::now();
	std::chrono::steady_clock::time_point _lastTime;
	float _totalTime = 0;
	std::chrono::steady_clock::time_point _lastFpsUpdate;
	const std::chrono::duration<double, std::chrono::seconds::period> _reportFpsRate{ 1 };
	FpsCounter _fpsCounter{};


	void InitWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't use opengl

		GLFWwindow* window = glfwCreateWindow(_defaultWindowSize.x, _defaultWindowSize.y, "Vulkan", nullptr, nullptr);
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

	
	void InitUI()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		
		ImGui::StyleColorsLight();

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan(_window, true);
		const char* glsl_version = "#version 450";// ? which version
		
		ImGui_ImplVulkan_Init(glsl_version);
	}



	// TODO Move to utils class
	static float RandF(float min, float max)
	{
		const auto base = float(rand()) / RAND_MAX;
		return min + base * (max - min);
	}
	
	#pragma region Scene Management

	bool _sceneLoaded = false;

	void LoadSphereArray()
	{
		const auto path = _options.ModelsDir + "sphere/sphere.obj";
		std::cout << "Loading model:" << path << std::endl;

		glm::vec3 center = { 0,4,0 };
		auto numRows = 2;
		auto numColumns = 5;
		auto rowSpacing = 2.2f;
		auto colSpacing = 2.2f;
		
		for (int row = 0; row < numRows; row++)
		{
			f32 metalness = row / f32(numRows - 1);

			f32 height = f32(numRows - 1) * rowSpacing;
			f32 hStart = -height / 2.f;
			f32 y = center.y + hStart + rowSpacing * row;

			for (int col = 0; col < numColumns; col++)
			{
				f32 roughness = col / f32(numColumns - 1);

				f32 width = f32(numColumns - 1) * colSpacing;
				f32 wStart = -width / 2.f;
				f32 x = center.x + wStart + colSpacing * col;

				auto entity = std::make_unique<Entity>();
				entity->Name = "Sphere_" + std::to_string(row) + "_" + std::to_string(col);
				entity->Transform.SetPos({ x,y,0.f });
				entity->Renderable = _scene->LoadRenderableComponentFromFile(path);

				// config mat
				Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);
				matCopy.Roughness = roughness;
				matCopy.Metalness = metalness;
				_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);

				_scene->GetEntities().emplace_back(std::move(entity));
			}
		}

	}
	
	void LoadSphere()
	{
		const auto path = _options.ModelsDir + "sphere/sphere.obj";
		std::cout << "Loading model:" << path << std::endl;

		auto entity = std::make_unique<Entity>();
		entity->Name = "Sphere";
		entity->Transform.SetPos(glm::vec3{ 0, 2, 0 });
		entity->Renderable = _scene->LoadRenderableComponentFromFile(path);

		// config mat
		Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);
		matCopy.Roughness = 0;
		matCopy.Metalness = 1;
		_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);

		_scene->GetEntities().emplace_back(std::move(entity));
	}
	
	void LoadRailgun()
	{
		const auto path = _options.ModelsDir + "railgun/q2railgun.gltf";
		std::cout << "Loading model:" << path << std::endl;

		auto& entities = _scene->GetEntities();

		auto entity = std::make_unique<Entity>();
		entity->Name = "Railgun";
		entity->Transform.SetPos(glm::vec3{ 0, -3, 0 });
		entity->Renderable = _scene->LoadRenderableComponentFromFile(path);


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


		entities.emplace_back(std::move(entity));
	}

	void LoadAxis()
	{
		std::cout << "Loading axis" << std::endl;

		auto scale = glm::vec3{ 0.5f };
		f32 dist = 1;
		const auto path = _options.ModelsDir + "sphere/sphere.obj";

		// Pivot
		{
			auto entity = std::make_unique<Entity>();
			entity->Name = "X";
			entity->Transform.SetScale(scale*0.5f);
			entity->Transform.SetPos(glm::vec3{ 0, 0, 0 });
			entity->Renderable = _scene->LoadRenderableComponentFromFile(path);

			Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);
			{
				matCopy.UseBasecolorMap = true;
				matCopy.BasecolorMap = _scene->LoadTexture(_options.AssetsDir + "Materials/ScuffedAluminum/BaseColor.png");

				matCopy.UseNormalMap = true;
				matCopy.NormalMap = _scene->LoadTexture(_options.AssetsDir + "Materials/ScuffedAluminum/Normal.png");
				
				matCopy.UseAoMap = true;
				matCopy.AoMap = _scene->LoadTexture(_options.AssetsDir + "Materials/ScuffedAluminum/ORM.png");
				matCopy.AoMapChannel = Material::Channel::Red;
				
				matCopy.UseRoughnessMap = true;
				matCopy.RoughnessMap = _scene->LoadTexture(_options.AssetsDir + "Materials/ScuffedAluminum/ORM.png");
				matCopy.RoughnessMapChannel = Material::Channel::Green;

				matCopy.UseMetalnessMap = true;
				matCopy.MetalnessMap = _scene->LoadTexture(_options.AssetsDir + "Materials/ScuffedAluminum/ORM.png");
				matCopy.MetalnessMapChannel = Material::Channel::Blue;
			}
			_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);

			_scene->GetEntities().emplace_back(std::move(entity));
		}
		
		// X
		{
			auto entity = std::make_unique<Entity>();
			entity->Name = "X";
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ dist, 0, 0 });
			entity->Renderable = _scene->LoadRenderableComponentFromFile(path);

			Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);
			{
				matCopy.Basecolor = { 1,0,0 };

				matCopy.UseNormalMap = true;
				matCopy.NormalMap = _scene->LoadTexture(_options.AssetsDir + "Materials/BumpyPlastic/Normal.png");

				matCopy.UseAoMap = true;
				matCopy.AoMap = _scene->LoadTexture(_options.AssetsDir + "Materials/BumpyPlastic/ORM.png");
				matCopy.AoMapChannel = Material::Channel::Red;

				matCopy.UseRoughnessMap = true;
				matCopy.RoughnessMap = _scene->LoadTexture(_options.AssetsDir + "Materials/BumpyPlastic/ORM.png");
				matCopy.RoughnessMapChannel = Material::Channel::Green;

				matCopy.UseMetalnessMap = true;
				matCopy.MetalnessMap = _scene->LoadTexture(_options.AssetsDir + "Materials/BumpyPlastic/ORM.png");
				matCopy.MetalnessMapChannel = Material::Channel::Blue;
			}
			_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);

			_scene->GetEntities().emplace_back(std::move(entity));
		}
		
		// Y
		{
			auto entity = std::make_unique<Entity>();
			entity->Name = "Y";
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ 0, dist, 0 });
			entity->Renderable = _scene->LoadRenderableComponentFromFile(path);
			Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);
			matCopy.Basecolor = { 0,1,0 };
			matCopy.Roughness = 0;
			_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);
			_scene->GetEntities().emplace_back(std::move(entity));
		}
		
		// Z
		{
			auto entity = std::make_unique<Entity>();
			entity->Name = "Z";
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ 0, 0, dist });
			entity->Renderable = _scene->LoadRenderableComponentFromFile(path);
			Material matCopy = _scene->GetMaterial(entity->Renderable->RenderableId);
			matCopy.Basecolor = { 0,0,1 };
			matCopy.Roughness = 0;
			_scene->SetMaterial(entity->Renderable->RenderableId, matCopy);
			_scene->GetEntities().emplace_back(std::move(entity));
		}
	}

	void NextSkybox()
	{
		_currentSkybox = ++_currentSkybox % _skyboxPaths.size();
		LoadSkybox(_skyboxPaths[_currentSkybox]);
	}
	
	void LoadSkybox(const std::string& filename)
	{
		std::string path = _options.IblDir + filename;

		std::cout << "Loading skybox: " << path << std::endl;

		SkyboxResourceId skyboxResourceId;
		
		// See if skybox is already loaded
		const auto it = _loadedSkyboxes.find(path);
		if (it != _loadedSkyboxes.end())
		{
			// Use existing resource
			skyboxResourceId = it->second;
		}
		else
		{
			std::cout << "  Creating skybox\n";

			// Create new resource
			const auto ids = _renderer->CreateIblTextureResources(path);
			SkyboxCreateInfo createInfo = {};
			createInfo.IblTextureIds = ids;
			skyboxResourceId = _renderer->CreateSkybox(createInfo);

			_loadedSkyboxes.emplace(path, skyboxResourceId);
		}
		

		_renderer->SetSkybox(skyboxResourceId);
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
		_scene->GetEntities().emplace_back(std::move(dirLight));

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
			_scene->GetEntities().emplace_back(std::move(light));
		}
	}

	void RandomizeLights()
	{
		for (auto& entity : _scene->GetEntities())
		{
			if (entity->Light.has_value())
			{
				entity->Transform.SetPos({ RandF(-10,10),RandF(-10,10),RandF(-10,10) });
				entity->Light->Color = { RandF(0,1),RandF(0,1),RandF(0,1) };
			}
		}
	}

	void LoadScene() 
	{
		if (_sceneLoaded) { return; }
		_sceneLoaded = true;

		std::cout << "Loading scene\n";
		LoadSkybox(_skyboxPaths[_currentSkybox]);
		LoadAxis();
		LoadSphereArray();
		//LoadStormtrooperHelmet();
		LoadRailgun();
		//LoadLighting();
	}

	void FrameAll()
	{
		// TODO Fix this method!
		return;
		/*
		// Nothing to frame?
		if (_entities.empty())
		{
			return; // TODO Setup default scene framing?
		}


		// Compute the bounds of all renderable's in the selection
		AABB totalBounds;
		bool first = true;
		for (auto& entity : _entities)
		{
			ModelResource modelRes = _resourceManager->GetModel(entity->Renderable.ModelId);
			auto localBounds = modelRes.Mesh->Bounds;
			
			auto worldBounds = localBounds.Transform(model->Transform.GetMatrix());

			if (first)
			{
				first = false;
				totalBounds = worldBounds;
			}
			else
			{
				totalBounds = totalBounds.Merge(worldBounds);
			}
		}


		if (first == true || totalBounds.IsEmpty())
		{
			return;
		}


		// Focus the bounds!
		auto center = totalBounds.Center();
		auto radius = glm::length(totalBounds.Max() - totalBounds.Min());
		const auto framebufferSize = GetFramebufferSize();
		const f32 viewportAspect = float(framebufferSize.width) / float(framebufferSize.height);
		_camera.Focus(center, radius, viewportAspect);
		*/
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
		if (key == GLFW_KEY_F)      { FrameAll(); }
		if (key == GLFW_KEY_X)      { LoadScene(); }
		if (key == GLFW_KEY_L)      { RandomizeLights(); }
		if (key == GLFW_KEY_C)      { NextSkybox(); }
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
	}

	#pragma endregion 
};
