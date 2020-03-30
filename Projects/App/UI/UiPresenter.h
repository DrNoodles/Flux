#pragma once

#include "PropsView/PropsView.h"
#include "SceneView.h"
#include "PropsView/LightVm.h"
#include "PropsView/TransformVm.h"

#include <State/SceneManager.h>


class LibraryManager;

class IUiPresenterDelegate
{
public:
	virtual ~IUiPresenterDelegate() = default;
	virtual glm::ivec2 GetWindowSize() const = 0;
	virtual void Delete(const std::vector<int>& entityIds) = 0;
	virtual void LoadDemoScene() = 0;
	virtual void LoadDemoSceneHeavy() = 0;
	virtual RenderOptions& GetRenderOptions() = 0;
	virtual void SetRenderOptions(const RenderOptions& ro) = 0;
};

class UiPresenter final : public ISceneViewDelegate, public IPropsViewDelegate
{
public:

	explicit UiPresenter(IUiPresenterDelegate& dgate, LibraryManager* library, SceneManager& scene) :
		_delegate(dgate),
		_scene{ scene },
		_library{ library },
		_sceneView{ SceneView{this} },
		_propsView{ PropsView{this} }
	{
	}

	~UiPresenter() = default;

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
	void Draw();

	// Fit to middle
	glm::ivec2 ViewportPos() const { return { _sceneViewWidth, 0 }; }
	glm::ivec2 ViewportSize() const { return { WindowWidth() - _propsViewWidth - _sceneViewWidth, WindowHeight() }; }
	
private:
	// Dependencies
	IUiPresenterDelegate& _delegate;
	SceneManager& _scene;
	LibraryManager* _library;

	
	// Views
	SceneView _sceneView;
	PropsView _propsView;

	
	// PropsView helpers
	int _selectionId = -1;
	int _selectedSubMesh = 0;
	std::vector<std::string> _submeshes{};
	TransformVm _tvm{}; // TODO Make optional and remove default constructor
	std::optional<LightVm> _lvm = std::nullopt;

	
	// Layout
	int _sceneViewWidth = 250;
	int _propsViewWidth = 300;

	int WindowWidth() const { return _delegate.GetWindowSize().x; }
	int WindowHeight() const { return _delegate.GetWindowSize().y; }
	// Anchor left
	glm::ivec2 ScenePos() const { return { 0, 0 }; }
	glm::ivec2 SceneSize() const
	{
		int i = _delegate.GetWindowSize().y;
		return { _sceneViewWidth, i };
	}

	// Anchor right
	glm::ivec2 PropsPos() const { return { WindowWidth() - _propsViewWidth,0 }; }
	glm::ivec2 PropsSize() const { return { _propsViewWidth, WindowHeight() }; }

	
	// Selection
	std::unordered_set<Entity*> _selection{};
	
	u32 _activeSkybox = 0;

	
	
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
	
	const RenderOptions& GetRenderOptions() override;
	void SetRenderOptions(const RenderOptions& ro) override;
	
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
