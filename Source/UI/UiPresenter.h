#pragma once

#include "ScenePane.h"
#include "IblVm.h"
#include "PropsView/PropsView.h"
#include "PropsView/TransformVm.h"
#include "PropsView/LightVm.h"
#include "PropsView/RenderableVm.h"
//#include "PropsView/PropsPresenter.h"

#include <App/LibraryManager.h>

#include <Shared/FileService.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>



class IUiPresenterDelegate
{
public:
	virtual ~IUiPresenterDelegate() = default;
	virtual glm::ivec2 GetWindowSize() const = 0;
	virtual void Delete(const std::vector<int>& entityIds) = 0;
	virtual void LoadDemoScene() = 0;
	virtual RenderOptions& GetRenderOptions() = 0;
	virtual void SetRenderOptions(const RenderOptions& ro) = 0;
};

class UiPresenter final : public ISceneViewDelegate, public IPropsViewDelegate
{
public:

	explicit UiPresenter(IUiPresenterDelegate& dgate, LibraryManager* library, SceneManager& scene)
	:
	_delegate(dgate),
	_scene{scene},
	_library{library},
	_scenePane{ScenePane{this}},
	_propsView{PropsView{this}}
	{
	}

	~UiPresenter() = default;

	void NextSkybox()
	{
		_activeSkybox = ++_activeSkybox % _library->GetSkyboxes().size();
		LoadSkybox(_library->GetSkyboxes()[_activeSkybox].Path);
	}

	void LoadSkybox(const std::string& path) const
	{
		const SkyboxResourceId resId = _scene.LoadSkybox(path);
		_scene.SetSkybox(resId);
	}

	void DeleteSelected() override
	{
		printf("DeleteSelected()\n");
		std::vector<int> ids{};
		std::for_each(_selection.begin(), _selection.end(), [&ids](Entity* s) { ids.emplace_back(s->Id); });
		_delegate.Delete(ids);
	}

	void FrameSelectionOrAll()
	{
		std::vector<Entity*> targets = {};

		
		if (_selection.empty()) // Frame scene
		{
			for (auto&& e : _scene.EntitiesView())
			{
				if (e->Renderable)
				{
					targets.emplace_back(e.get());
				}
			}
		}
		else // Frame selection
		{
			for (auto&& e : _selection)
			{
				if (e->Renderable)
				{
					targets.emplace_back(e);
				}
			}
		}

		
		// Nothing to frame?
		if (targets.empty())
		{
			return;
		}


		// Compute the world space bounds of all renderable's in the selection
		AABB totalWorldBounds;
		bool first = true;

		for (auto& entity : targets)
		{
			auto localBounds = entity->Renderable->GetBounds();
			auto worldBounds = localBounds.Transform(entity->Transform.GetMatrix());

			if (first)
			{
				first = false;
				totalWorldBounds = worldBounds;
			}
			else
			{
				totalWorldBounds = totalWorldBounds.Merge(worldBounds);
			}
		}

		if (first == true || totalWorldBounds.IsEmpty())
		{
			return;
		}

		// Focus the bounds
		auto radius = glm::length(totalWorldBounds.Max() - totalWorldBounds.Min());
		const auto aspect = float(ViewportSize().x) / float(ViewportSize().y);
		_scene.GetCamera().Focus(totalWorldBounds.Center(), radius, aspect);
	}

	
	// Disable copy
	UiPresenter(const UiPresenter&) = delete;
	UiPresenter& operator=(const UiPresenter&) = delete;

	// Disable move
	UiPresenter(UiPresenter&&) = delete;
	UiPresenter& operator=(UiPresenter&&) = delete;

	void ReplaceSelection(Entity*const entity)
	{
		ClearSelection();
		_selection.insert(entity);
	}
	void ClearSelection()
	{
		_selection.clear();
	}


	void Draw()
	{
		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		{
			//auto show_demo_window = true;
			//ImGui::ShowDemoWindow(&show_demo_window);

			
			// Scene Pane
			{
				ImGui::SetNextWindowPos(ImVec2(float(ScenePos().x), float(ScenePos().y)));
				ImGui::SetNextWindowSize(ImVec2(float(SceneSize().x), float(SceneSize().y)));

				auto& entsView = _scene.EntitiesView();
				std::vector<Entity*> allEnts{ entsView.size() };
				std::transform(entsView.begin(), entsView.end(), allEnts.begin(), [](const std::unique_ptr<Entity>& pe)
					{
						return pe.get();
					});
				IblVm iblVm{ &_scene, &_delegate.GetRenderOptions() };
				_scenePane.DrawUI(allEnts, _selection, iblVm);
			}

			
			// Properties Pane
			{
				ImGui::SetNextWindowPos(ImVec2(float(PropsPos().x), float(PropsPos().y)));
				ImGui::SetNextWindowSize(ImVec2(float(PropsSize().x), float(PropsSize().y)));

				Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;

				auto selectionCount = (int)_selection.size();
				if (selectionCount != 1)
				{
					// Reset it all
					_selectionId = -1;
					_tvm = TransformVm{};
					_rvm = std::nullopt;
					_lvm = std::nullopt;
				}
				else if (selection && selection->Id != _selectionId)
				{
					// New selection
					_selectionId = selection->Id;
					_tvm = TransformVm{ &selection->Transform };

					_lvm = selection->Light.has_value()
						? std::optional(LightVm{ &selection->Light.value() })
						: std::nullopt;

					if (selection->Renderable.has_value())
					{
						const auto& renComp = *selection->Renderable;
						const auto& submeshResId = renComp.GetMeshIds()[_selectedSubMesh];
						const auto& mat = _scene.GetMaterial(submeshResId);
						
						_rvm = std::optional(PopulateMaterialState(mat));
					}
					else
					{
						_rvm = std::nullopt;
					}
				}
				else
				{
					// Same selection as last frame
				}

				_propsView.DrawUI(selectionCount, _tvm, _rvm, _lvm);
			}
		}
		ImGui::EndFrame();
		ImGui::Render();
	}

	
private:
	// Dependencies
	IUiPresenterDelegate& _delegate;
	SceneManager& _scene;
	LibraryManager* _library;

	// Views
	ScenePane _scenePane;
	PropsView _propsView;
	//PropsPresenter _propsPresenter;

	// PropsView helpers
	int _selectionId = -1;
	int _selectedSubMesh = 0;
	std::vector<std::string> _submeshes{};
	TransformVm _tvm{}; // TODO Make optional and remove default constructor
	std::optional<RenderableVm> _rvm = std::nullopt;
	std::optional<LightVm> _lvm = std::nullopt;

	
	// Layout
	int _scenePanelWidth = 250;
	int _propsPanelWidth = 300;

	int WindowWidth() const { return _delegate.GetWindowSize().x; }
	int WindowHeight() const { return _delegate.GetWindowSize().y; }
	// Anchor left
	glm::ivec2 ScenePos() const { return { 0, 0 }; }
	glm::ivec2 SceneSize() const
	{
		int i = _delegate.GetWindowSize().y;
		return { _scenePanelWidth, i };
	}
	// Fit to middle
	glm::ivec2 ViewportPos() const { return { _scenePanelWidth, 0 }; }
	glm::ivec2 ViewportSize() const { return { WindowWidth() - _propsPanelWidth - _scenePanelWidth, WindowHeight() }; }
	// Anchor right
	glm::ivec2 PropsPos() const { return { WindowWidth() - _propsPanelWidth,0 }; }
	glm::ivec2 PropsSize() const { return { _propsPanelWidth, WindowHeight() }; }


	// Selection -- TODO Find a better home for this?
	std::unordered_set<Entity*> _selection{};
	
	
	#pragma region ISceneViewDelegate
	
	void LoadDemoScene() override
	{
		printf("LoadDemoScene()\n");
		_delegate.LoadDemoScene();
		ClearSelection();
		FrameSelectionOrAll();
	}
	void LoadModel(const std::string& path) override
	{
		printf("LoadModel(%s)\n", path.c_str());

		// Split path into a dir and filename so we can name the entity
		std::string dir, filename;
		std::tie(dir, filename) = FileService::SplitPathAsDirAndFilename(path);

		
		// Create new entity
		auto entity = std::make_unique<Entity>();
		entity->Name = filename;
		entity->Transform.SetPos(glm::vec3{ 0, -3, 0 });
		entity->Renderable = _scene.LoadRenderableComponentFromFile(path);

		//_scene.SetMaterial(*entity->Renderable, LibraryManager::CreateRandomMaterial());
		
		ReplaceSelection(entity.get());
		
		_scene.AddEntity(std::move(entity));

		FrameSelectionOrAll();
	}
	void CreateDirectionalLight() override
	{
		printf("CreateDirectionalLight()\n");

		auto entity = std::make_unique<Entity>();
		entity->Name = "DirectionalLight" + std::to_string(entity->Id);
		entity->Light = LightComponent{};
		entity->Light->Type = LightComponent::Types::directional;
		entity->Light->Intensity = 15;
		entity->Transform.SetPos({ 1, 1, 1 });

		ReplaceSelection(entity.get());
		_scene.AddEntity(std::move(entity));
	}
	void CreatePointLight() override
	{
		printf("CreatePointLight()\n");
		
		auto entity = std::make_unique<Entity>();
		entity->Name = "PointLight" + std::to_string(entity->Id);
		entity->Light = LightComponent{};
		entity->Light->Type = LightComponent::Types::point;
		entity->Light->Intensity = 500;

		ReplaceSelection(entity.get());
		_scene.AddEntity(std::move(entity));
	}
	void CreateSphere() override
	{
		printf("CreateSphere()\n");

		auto entity = _library->CreateSphere();
		//entity->Action = std::make_unique<TurntableAction>(entity->Transform);

		ReplaceSelection(entity.get());
		_scene.AddEntity(std::move(entity));
	}
	void CreateBlob() override
	{
		printf("CreateBlob()\n");

		auto entity = _library->CreateBlob();
		entity->Action = std::make_unique<TurntableAction>(entity->Transform);

		ReplaceSelection(entity.get());
		_scene.AddEntity(std::move(entity));
	}
	void CreateCube() override
	{
		printf("CreateCube()\n");

		auto entity = _library->CreateCube();
		entity->Transform.SetScale(glm::vec3{ 1.5f });
		entity->Action = std::make_unique<TurntableAction>(entity->Transform);

		ReplaceSelection(entity.get());
		_scene.AddEntity(std::move(entity));
	}

	void DeleteAll() override
	{
		printf("DeleteAll()\n");
		auto& entities = _scene.EntitiesView();
		std::vector<int> ids{};
		std::for_each(entities.begin(), entities.end(), [&ids](const std::unique_ptr<Entity>& e) { ids.emplace_back(e->Id); });
		_delegate.Delete(ids);

	}

	float GetExposure() const override
	{
		//printf("GetExposure()\n");
		return _delegate.GetRenderOptions().ExposureBias;
	}
	void SetExposure(float exposure) override
	{
		printf("SetExposure(%f)\n", exposure);

		auto ro = _delegate.GetRenderOptions();
		ro.ExposureBias = exposure;
		_delegate.SetRenderOptions(ro);
	}
	
	float GetSkyboxRotation() const override
	{
		return _delegate.GetRenderOptions().SkyboxRotation;
	}
	void SetSkyboxRotation(float rotation) override
	{
		printf("SetSkyboxRotation(%f)\n", rotation);

		auto ro = _delegate.GetRenderOptions();
		ro.SkyboxRotation = rotation;
		_delegate.SetRenderOptions(ro);
	}

	u32 _activeSkybox = 0;
	const std::vector<SkyboxInfo>& GetSkyboxList() override
	{
		return _library->GetSkyboxes();
	}
	u32 GetActiveSkybox() const override { return _activeSkybox; }
	void SetActiveSkybox(u32 idx) override
	{
		const auto& skyboxInfo = _library->GetSkyboxes()[idx];
		const auto resourceId = _scene.LoadSkybox(skyboxInfo.Path);
		_scene.SetSkybox(resourceId);

		_activeSkybox = idx;
	}

	#pragma endregion




	RenderableVm PopulateMaterialState(const Material& mat)
	{
		RenderableVm rvm = {};

		rvm.UseBaseColorMap = mat.UseBasecolorMap;
		rvm.UseMetalnessMap = mat.UseMetalnessMap;
		rvm.UseRoughnessMap = mat.UseRoughnessMap;
		//rvm.UseNormalMap = mat.UseNormalMap;
		//rvm.UseAoMap = mat.UseAoMap;

		rvm.BaseColor = mat.Basecolor;
		rvm.Metalness = mat.Metalness;
		rvm.Roughness = mat.Roughness;

		rvm.InvertNormalMapZ = mat.InvertNormalMapZ;
		rvm.InvertAoMap = mat.InvertAoMap;
		rvm.InvertRoughnessMap = mat.InvertRoughnessMap;
		rvm.InvertMetalnessMap = mat.InvertMetalnessMap;

		rvm.ActiveMetalnessChannel = int(mat.MetalnessMapChannel);
		rvm.ActiveRoughnessChannel = int(mat.RoughnessMapChannel);
		rvm.ActiveAoChannel = int(mat.AoMapChannel);

		rvm.ActiveSolo = mat.ActiveSolo;

		rvm.BaseColorMapPath = mat.HasBasecolorMap() ? mat.BasecolorMapPath : "";
		rvm.NormalMapPath = mat.HasNormalMap() ? mat.NormalMapPath : "";
		rvm.MetalnessMapPath = mat.HasMetalnessMap() ? mat.MetalnessMapPath : "";
		rvm.RoughnessMapPath = mat.HasRoughnessMap() ? mat.RoughnessMapPath : "";
		rvm.AoMapPath = mat.HasAoMap() ? mat.AoMapPath : "";

		return rvm;
	}


	#pragma region IPropsViewDelegate
	
	RenderableVm GetMaterialState() const override
	{
		return RenderableVm();
	}
	void CommitMaterialChanges(RenderableVm state) override
	{
		
	}
	int GetSelectedSubMesh() const override { return _selectedSubMesh; }
	void SelectSubMesh(int index) override { _selectedSubMesh = index; }
	const std::vector<std::string>& GetSubmeshes() override { return _submeshes; }

	#pragma endregion
	
};
