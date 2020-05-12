#pragma once

#include "UiPresenterHelpers.h"

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
	virtual glm::ivec2 GetWindowSize() const = 0;
	virtual void Delete(const std::vector<int>& entityIds) = 0;
	virtual void LoadDemoScene() = 0;
	virtual void LoadDemoSceneHeavy() = 0;
	virtual void CloseApp() = 0;
};

class UiPresenter final : public ISceneViewDelegate, public IPropsViewDelegate, public IViewportViewDelegate
{
public: // DATA
	
private: // DATA
	// Dependencies
	IUiPresenterDelegate& _delegate;
	SceneManager& _scene;
	LibraryManager& _library;
	Renderer& _renderer; // temp, move to ViewportView
	VulkanService& _vulkan; // temp, remove

	// Views
	SceneView _sceneView;
	PropsView _propsView;
	ViewportView _viewportView;

	// PropsView helpers
	int _selectionId = -1;
	int _selectedSubMesh = 0;
	std::vector<std::string> _submeshes{};
	TransformVm _tvm{}; // TODO Make optional and remove default constructor
	std::optional<LightVm> _lvm = std::nullopt;

	u32 _activeSkybox = 0;
	RenderOptions _renderOptions;
	std::unordered_set<Entity*> _selection{};

	// Layout
	int _sceneViewWidth = 250;
	int _propsViewWidth = 300;

	// UI Timer
	std::chrono::steady_clock::time_point _lastUiUpdate;
	std::chrono::duration<float, std::chrono::seconds::period> _uiUpdateRate{ 1.f / 90 };

	
	// Rendering shit - TODO Move these graphics impl deets out of this UI class somehow

	//std::unique_ptr<TextureResource> _offscreenTextureResource = nullptr;
	//UiPresenterHelpers::FramebufferResources _offscreenFramebuffer;
	
	//UiPresenterHelpers::PostPassResources _postPassResources;

	

public: // METHODS
	UiPresenter(IUiPresenterDelegate& dgate, LibraryManager& library, SceneManager& scene, Renderer& renderer, VulkanService& vulkan, const std::string& shaderDir);
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
	
	void BeginQuitPrompt();

	void SetUpdateRate(int updatesPerSecond)
	{
		_uiUpdateRate = std::chrono::duration<float, std::chrono::seconds::period>{ 1.f/float(updatesPerSecond) };
	}
	void Draw(u32 imageIndex, VkCommandBuffer commandBuffer); 

	// Fit to middle
	glm::ivec2 ViewportPos() const { return { _sceneViewWidth, 0 }; }
	glm::ivec2 ViewportSize() const { return { WindowWidth() - _propsViewWidth - _sceneViewWidth, WindowHeight() }; }

	// Temp public - just pub so App can build a scene up and set render options. TODO remove scene buildng from App alltogether
	const RenderOptions& GetRenderOptions() override;
	void SetRenderOptions(const RenderOptions& ro) override;
	
private: // METHODS
	int WindowWidth() const { return _delegate.GetWindowSize().x; }
	int WindowHeight() const { return _delegate.GetWindowSize().y; }
	// Anchor left
	glm::ivec2 ScenePos() const { return { 0, 0 }; }
	glm::ivec2 SceneSize() const { return { _sceneViewWidth, _delegate.GetWindowSize().y }; }

	// Anchor right
	glm::ivec2 PropsPos() const { return { WindowWidth() - _propsViewWidth,0 }; }
	glm::ivec2 PropsSize() const { return { _propsViewWidth, WindowHeight() }; }

	void BuildImGui();
	void DrawViewport(u32 imageIndex, VkCommandBuffer commandBuffer);
	void DrawUi(VkCommandBuffer commandBuffer);

	
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
	
	//const RenderOptions& GetRenderOptions() override;
	//void SetRenderOptions(const RenderOptions& ro) override;

	void LoadSkybox() override;
	float GetSkyboxRotation() const override;
	void SetSkyboxRotation(float rotation) override;
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
	static MaterialViewState PopulateMaterialState(const Material& mat);

	#pragma endregion
};
