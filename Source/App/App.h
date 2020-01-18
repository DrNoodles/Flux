#pragma once

#include "IModelLoaderService.h"
#include "AssImpModelLoaderService.h"
#include "Camera.h"
#include "AppTypes.h"
#include "SceneManager.h"
#include "Entity/Entity.h"

#include "Shared/AABB.h"
#include "Renderer/Renderer.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stbi/stb_image.h>

#include <iostream>
#include <utility>
#include <vector>
#include <algorithm>
#include <chrono>
#include <string>
#include <iomanip>
#include <unordered_map>


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
		_renderer = std::make_unique<Renderer>(_options.EnabledVulkanValidationLayers, _options.ShaderDir, *this);
		_modelLoaderService = std::make_unique<AssimpModelLoaderService>();
		_sceneManager = std::make_unique<SceneManager>(*_modelLoaderService, *_renderer);
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
		const auto degreesPerSec = 60.f;
		const auto rotationDelta = dt * degreesPerSec;

		for (const auto& entity : _entities)
		{
			auto& transform = entity->Transform;

			auto rot = transform.GetRot();
			rot.y += rotationDelta;
			transform.SetRot(rot);
		}
	}

	
	void Draw(const float dt) const
	{
		std::vector<ModelResourceId> models(_entities.size());
		std::vector<glm::mat4> transforms(_entities.size());
		
		for (size_t i = 0; i < _entities.size(); i++)
		{
			models[i] = _entities[i]->Renderable.ModelResId;
			transforms[i] = _entities[i]->Transform.GetMatrix();
		}

		_renderer->DrawFrame(dt, models, transforms, _camera.GetViewMatrix());
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

	#pragma endregion

	
private:
	// Dependencies
	std::unique_ptr<Renderer> _renderer = nullptr;
	std::unique_ptr<SceneManager> _sceneManager = nullptr;
	std::unique_ptr<IModelLoaderService> _modelLoaderService = nullptr;
	
	const glm::ivec2 _defaultWindowSize = { 800,600 };
	GLFWwindow* _window = nullptr;
	AppOptions _options;

	
	// Scene
	Camera _camera;
	std::vector<std::unique_ptr<Entity>> _entities{};

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

	
	#pragma region Scene Management

	bool _assetLoaded = false;
	void LoadAssets() 
	{
		//if (_assetLoaded) { return; }
		//_assetLoaded = true;

		const auto path = _options.AssetsDir + "railgun/q2railgun.gltf";
		std::cout << "Loading model:" << path << std::endl;
		
		auto entity = std::make_unique<Entity>();
		entity->Transform.SetPos(glm::vec3{ _entities.size(), 0, 0 });
		entity->Renderable = _sceneManager->LoadRenderableComponentFromFile(path);
		_entities.emplace_back(std::move(entity));
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
		_camera.ProcessMouseScroll(float(yOffset));
	}
	void OnKeyCallback(int key, int scancode, int action, int mods)
	{
		// ONLY on pressed is handled
		if (action == GLFW_REPEAT || action == GLFW_RELEASE) return;

		if (key == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose(_window, 1); }
		if (key == GLFW_KEY_F)      { FrameAll(); }
		if (key == GLFW_KEY_X)      { LoadAssets(); }
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
		if (isLmb)
		{
			//_camera.ProcessMouseMovement(float(xDiff), float(yDiff));

			const float arcSpeed = 3;
			_camera.Arc(diffRatio.x * arcSpeed, diffRatio.y * arcSpeed);
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
				_camera.Move(len, glm::normalize(dir), speed);
			}
		}
	}
	void OnWindowSizeChanged(int width, int height)
	{
		FramebufferResized = true;
	}

	#pragma endregion 
};