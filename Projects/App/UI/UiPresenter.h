#pragma once

#include "IWindow.h"
#include "OnScreen.h"
#include "OffScreen.h"

#include "PropsView/LightVm.h"
#include "PropsView/PropsView.h"
#include "PropsView/TransformVm.h"
#include "SceneView/SceneView.h"
#include "ViewportView/IViewportViewDelegate.h"
#include "ViewportView/ViewportView.h"

#include <chrono>



class LibraryManager;
class SceneManager;
class Renderer;

class IUiPresenterDelegate
{
public:
	virtual ~IUiPresenterDelegate() = default;
	virtual void Delete(const std::vector<int>& entityIds) = 0;
	virtual void ToggleUpdateEntities() = 0;
};

class UiPresenter final :
	public ISceneViewDelegate,
	public IPropsViewDelegate,
	public IViewportViewDelegate
{
public: // DATA

private: // DATA

	// Dependencies
	IUiPresenterDelegate& _delegate;
	SceneManager& _scene;
	LibraryManager& _library;
	Renderer& _renderer; // temp, move to ViewportView
	VulkanService& _vulkan; // temp, remove
	IWindow* _window = nullptr;
	std::string _shaderDir = {};

	// Views
	SceneView _sceneView;
	PropsView _propsView;
	ViewportView _viewportView;

	bool _firstCursorInput = true;
	f64 _lastCursorX{}, _lastCursorY{};

	// PropsView helpers
	int _selectionId = -1;
	int _selectedSubMesh = 0;
	std::vector<std::string> _submeshes{};
	TransformVm _tvm{}; // TODO Make optional and remove default constructor
	std::optional<LightVm> _lvm = std::nullopt;

	u32 _activeSkybox = 0;
	std::unordered_set<Entity*> _selection{};

	// Layout
	u32 _sceneViewWidth = 250;
	u32 _propsViewWidth = 300;

	// UI Timer
	std::chrono::steady_clock::time_point _lastUiUpdate;
	std::chrono::duration<float, std::chrono::seconds::period> _uiUpdateRate{ 1.f / 90 };


	// Rendering shit - TODO Move these graphics impl deets out of this UI class somehow
	OffScreen::FramebufferResources _sceneFramebuffer;
	OnScreen::QuadResources _postPassResources;

	WindowSizeChangedDelegate _windowSizeChangedHandler = [this](auto* s, auto a) { OnWindowSizeChanged(s, a); };
	PointerMovedDelegate _pointerMovedHandler = [this](auto* s, auto a) { OnPointerMoved(s, a); };
	PointerWheelChangedDelegate _pointerWheelChangedHandler = [this](auto* s, auto a) { OnPointerWheelChanged(s, a); };
	KeyDownDelegate _keyDownHandler = [this](auto* s, auto a) { OnKeyDown(s, a); };
	KeyUpDelegate _keyUpHandler = [this](auto* s, auto a) { OnKeyUp(s, a); };

public: // METHODS

	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
	{
		// This is very heavy handed asset recreation. TODO Optimise this
		
		DestroySceneFramebuffer();
		DestroyQuadResources();

		CreateSceneFramebuffer();
		CreateQuadResources(_sceneFramebuffer.OutputDescriptor, _shaderDir, _vulkan.GetSwapchain()); //Code smell: This has hidden dependencies on scene framebuffer 
	}

	
	UiPresenter(IUiPresenterDelegate& dgate, LibraryManager& library, SceneManager& scene, Renderer& renderer, VulkanService& vulkan, IWindow* window, const std::string& shaderDir);
	~UiPresenter() = default;
	
	void Shutdown();
	// Disable copy
	UiPresenter(const UiPresenter&) = delete;
	UiPresenter& operator=(const UiPresenter&) = delete;
	// Disable move
	UiPresenter(UiPresenter&&) = delete;
	UiPresenter& operator=(UiPresenter&&) = delete;

	void NextSkybox();
	void LoadSkybox(const std::string& path) const;
	void DeleteSelected() override;
	void FrameSelectionOrAll();
	void ReplaceSelection(Entity* const entity);
	void ClearSelection();

	void SetUpdateRate(int updatesPerSecond)
	{
		_uiUpdateRate = std::chrono::duration<float, std::chrono::seconds::period>{ 1.f / float(updatesPerSecond) };
	}

	void Draw(u32 imageIndex, VkCommandBuffer commandBuffer);

private: // METHODS

	void CreateSceneFramebuffer()
	{
		// Scene framebuffer is only the size of the scene render region on screen
		const auto sceneRect = ViewportRect();
		const auto sceneExtent = VkExtent2D{ sceneRect.Extent.Width, sceneRect.Extent.Height };
		
		_sceneFramebuffer = OffScreen::CreateSceneOffscreenFramebuffer(
		sceneExtent, 
		VK_FORMAT_R16G16B16A16_SFLOAT,
		_renderer.GetRenderPass(),
		_vulkan.MsaaSamples(),
		_vulkan.LogicalDevice(), _vulkan.PhysicalDevice());
	}
	void DestroySceneFramebuffer()
	{
		_sceneFramebuffer.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	}


	void CreateQuadResources(const VkDescriptorImageInfo& sceneGbuf, const std::string& shaderDir, const Swapchain& swapchain);
	/*void UpdateQuadResources(const VkDescriptorImageInfo& sceneGbuf)
	{
		
	}*/
	void DestroyQuadResources()
	{
		_postPassResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	}
	
	// Anchor Scene-view to left
	Rect2D SceneRect() const
	{
		Rect2D r;
		r.Offset = { 0,0 };
		r.Extent = { _sceneViewWidth, _window->GetSize().Height };
		return r;
	}

	// Fit Viewport to middle
	Rect2D ViewportRect() const
	{
		Rect2D r;
		r.Offset = { (i32)SceneRect().Extent.Width, 0 };
		r.Extent = { (u32)(PropsRect().Offset.X - r.Offset.X), _window->GetSize().Height };
		return r;
	}

	// Anchor Props-view to right
	Rect2D PropsRect() const
	{
		Rect2D r;
		r.Offset = { (i32)(_window->GetSize().Width - _propsViewWidth), 0 };
		r.Extent = { _propsViewWidth, _window->GetSize().Height };
		return r;
	}

	void BuildImGui();
	void DrawViewport(u32 imageIndex, VkCommandBuffer commandBuffer);
	void DrawPostProcessedViewport(VkCommandBuffer commandBuffer, i32 imageIndex);
	void DrawUi(VkCommandBuffer commandBuffer);



	// Event handlers
	void OnKeyDown(IWindow* sender, KeyEventArgs args);
	void OnKeyUp(IWindow* sender, KeyEventArgs args);
	void OnPointerWheelChanged(IWindow* sender, PointerEventArgs args);
	void OnPointerMoved(IWindow* sender, PointerEventArgs args);
	void OnWindowSizeChanged(IWindow* sender, WindowSizeChangedEventArgs args);


#pragma region ISceneViewDelegate

	void LoadDemoScene() override;
	void LoadHeavyDemoScene() override;
	void LoadModel(const std::string& path) override;

	void CreateDirectionalLight() override;
	void CreatePointLight() override;
	void CreateSphere() override;
	void CreateBlob() override;
	void CreateCube() override;

	void DeleteAll() override;

	RenderOptions GetRenderOptions() override;
	void SetRenderOptions(const RenderOptions& ro) override;
	void LoadAndSetSkybox() override;
	const std::vector<SkyboxInfo>& GetSkyboxList() override;
	u32 GetActiveSkybox() const override { return _activeSkybox; }
	void SetActiveSkybox(u32 idx) override;

#pragma endregion


#pragma region IPropsViewDelegate

	std::optional<MaterialViewState> GetMaterialState() override;
	void CommitMaterialChanges(const MaterialViewState& state) override;
	int GetSelectedSubMesh() const override { return _selectedSubMesh; }
	void SelectSubMesh(int index) override { _selectedSubMesh = index; }
	const std::vector<std::string>& GetSubmeshes() override { return _submeshes; }

#pragma endregion
};
