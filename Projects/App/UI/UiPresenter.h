#pragma once

#include "SceneView.h"
#include "IblVm.h"
#include "PropsView/PropsView.h"
#include "PropsView/TransformVm.h"
#include "PropsView/LightVm.h"
#include "PropsView/MaterialViewState.h"

#include <State/LibraryManager.h>
#include <State/Entity/Actions/TurntableActionComponent.h>

#include <Framework/FileService.h>

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
	virtual void LoadDemoSceneHeavy() = 0;
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
	_sceneView{SceneView{this}},
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

			
			// Scene View
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
				_sceneView.DrawUI(allEnts, _selection, iblVm);
			}

			
			// Properties View
			{
				ImGui::SetNextWindowPos(ImVec2(float(PropsPos().x), float(PropsPos().y)));
				ImGui::SetNextWindowSize(ImVec2(float(PropsSize().x), float(PropsSize().y)));

				Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;

				const auto selectionCount = (int)_selection.size();
				if (selectionCount != 1)
				{
					// Reset it all
					_selectionId = -1;
					_tvm = TransformVm{};
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

					
					// Collect submeshes
					_selectedSubMesh = 0;
					_submeshes.clear();
					if (selection->Renderable.has_value())
					{	
						for (const auto& componentSubmesh : selection->Renderable->GetSubmeshes())
						{
							_submeshes.emplace_back(componentSubmesh.Name);
						}
					}
				}
				else
				{
					// Same selection as last frame
				}

				_propsView.DrawUI(selectionCount, _tvm, _lvm);
			}
		}
		ImGui::EndFrame();
		ImGui::Render();
	}

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
	

	
	#pragma region ISceneViewDelegate
	
	void LoadDemoScene() override
	{
		printf("LoadDemoScene()\n");
		_delegate.LoadDemoScene();
		ClearSelection();
		FrameSelectionOrAll();
	}
	void LoadHeavyDemoScene() override
	{
		printf("LoadHeavyDemoScene()\n");
		_delegate.LoadDemoSceneHeavy();
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

	const RenderOptions& GetRenderOptions() override
	{
		return _delegate.GetRenderOptions();
	}
	void SetRenderOptions(const RenderOptions& ro) override
	{
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



	#pragma region IPropsViewDelegate

	std::optional<MaterialViewState> GetMaterialState() override
	{
		Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;

		if (!selection || !selection->Renderable.has_value())
			return std::nullopt;

		const auto& componentSubmesh = selection->Renderable->GetSubmeshes()[_selectedSubMesh];
		const auto& mat = _scene.GetMaterial(componentSubmesh.Id);

		return PopulateMaterialState(mat);
	}
	void CommitMaterialChanges(const MaterialViewState& state) override
	{
		Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
		if (!selection)
		{
			throw std::runtime_error("How are we commiting a material change when there's no valid selection?");
		}

		const auto& renComp = *selection->Renderable;
		const auto& componentSubmesh = renComp.GetSubmeshes()[_selectedSubMesh];
		auto mat = _scene.GetMaterial(componentSubmesh.Id);

		

		// Update material properties

		mat.UseBasecolorMap = state.UseBasecolorMap;
		mat.UseMetalnessMap = state.UseMetalnessMap;
		mat.UseRoughnessMap = state.UseRoughnessMap;
		//mat.UseNormalMap = state.UseNormalMap;
		//mat.UseAoMap = state.UseAoMap;
		
		mat.Basecolor = state.Basecolor;
		mat.Metalness = state.Metalness;
		mat.Roughness = state.Roughness;

		mat.InvertNormalMapZ = state.InvertNormalMapZ;
		mat.InvertAoMap = state.InvertAoMap;
		mat.InvertRoughnessMap = state.InvertRoughnessMap;
		mat.InvertMetalnessMap = state.InvertMetalnessMap;

		mat.MetalnessMapChannel = (Material::Channel)state.ActiveMetalnessChannel;
		mat.RoughnessMapChannel = (Material::Channel)state.ActiveRoughnessChannel;
		mat.AoMapChannel = (Material::Channel)state.ActiveAoChannel;

		switch (state.ActiveSolo)
		{
		case 0: mat.ActiveSolo = TextureType::Undefined; break;
		case 1: mat.ActiveSolo = TextureType::Basecolor; break;
		case 2: mat.ActiveSolo = TextureType::Metalness; break;
		case 3: mat.ActiveSolo = TextureType::Roughness; break;
		case 4: mat.ActiveSolo = TextureType::AmbientOcclusion; break;
		case 5: mat.ActiveSolo = TextureType::Normals; break;
		default:
			throw std::out_of_range("");
		}
		
		auto UpdateMap = [&](const std::string& newPath, Material& targetMat, const TextureType type)
		{
			std::optional<TextureResourceId>* pMap = nullptr;
			std::string* pMapPath = nullptr;

			switch (type)
			{
			case TextureType::Basecolor:
				pMap = &targetMat.BasecolorMap;
				pMapPath = &targetMat.BasecolorMapPath;
				break;
			case TextureType::Normals:
				pMap = &targetMat.NormalMap;
				pMapPath = &targetMat.NormalMapPath;
				break;
			case TextureType::Roughness:
				pMap = &targetMat.RoughnessMap;
				pMapPath = &targetMat.RoughnessMapPath;
				break;
			case TextureType::Metalness:
				pMap = &targetMat.MetalnessMap;
				pMapPath = &targetMat.MetalnessMapPath;
				break;
			case TextureType::AmbientOcclusion:
				pMap = &targetMat.AoMap;
				pMapPath = &targetMat.AoMapPath;
				break;

			case TextureType::Undefined:
			default:
				throw std::invalid_argument("unhandled TextureType");
			}

			if (newPath.empty())
			{
				*pMap = std::nullopt;
				*pMapPath = "";
			}
			else // path is empty, make sure the material is also
			{
				const bool pathIsDifferent = !(pMapPath && *pMapPath == newPath);
				if (pathIsDifferent)
				{
					*pMap = _scene.LoadTexture(newPath);;
					*pMapPath = newPath;
				}
			}
		};

		UpdateMap(state.BasecolorMapPath, mat, TextureType::Basecolor);
		UpdateMap(state.NormalMapPath, mat, TextureType::Normals);
		UpdateMap(state.MetalnessMapPath, mat, TextureType::Metalness);
		UpdateMap(state.RoughnessMapPath, mat, TextureType::Roughness);
		UpdateMap(state.AoMapPath, mat, TextureType::AmbientOcclusion);

		
		_scene.SetMaterial(componentSubmesh.Id, mat);
	}
	int GetSelectedSubMesh() const override { return _selectedSubMesh; }
	void SelectSubMesh(int index) override { _selectedSubMesh = index; }
	const std::vector<std::string>& GetSubmeshes() override { return _submeshes; }

	static MaterialViewState PopulateMaterialState(const Material& mat)
	{
		MaterialViewState rvm = {};

		rvm.UseBasecolorMap = mat.UseBasecolorMap;
		rvm.UseMetalnessMap = mat.UseMetalnessMap;
		rvm.UseRoughnessMap = mat.UseRoughnessMap;
		//rvm.UseNormalMap = mat.UseNormalMap;
		//rvm.UseAoMap = mat.UseAoMap;

		rvm.Basecolor = mat.Basecolor;
		rvm.Metalness = mat.Metalness;
		rvm.Roughness = mat.Roughness;

		rvm.InvertNormalMapZ = mat.InvertNormalMapZ;
		rvm.InvertAoMap = mat.InvertAoMap;
		rvm.InvertRoughnessMap = mat.InvertRoughnessMap;
		rvm.InvertMetalnessMap = mat.InvertMetalnessMap;

		rvm.ActiveMetalnessChannel = int(mat.MetalnessMapChannel);
		rvm.ActiveRoughnessChannel = int(mat.RoughnessMapChannel);
		rvm.ActiveAoChannel = int(mat.AoMapChannel);

		switch (mat.ActiveSolo)
		{
		case TextureType::Undefined: rvm.ActiveSolo = 0; break;
		case TextureType::Basecolor: rvm.ActiveSolo = 1; break;
		case TextureType::Metalness: rvm.ActiveSolo = 2; break;
		case TextureType::Roughness: rvm.ActiveSolo = 3; break;
		case TextureType::AmbientOcclusion: rvm.ActiveSolo = 4; break;
		case TextureType::Normals: rvm.ActiveSolo = 5; break;
		default:
			throw std::out_of_range("");
		}

		rvm.BasecolorMapPath = mat.HasBasecolorMap() ? mat.BasecolorMapPath : "";
		rvm.NormalMapPath = mat.HasNormalMap() ? mat.NormalMapPath : "";
		rvm.MetalnessMapPath = mat.HasMetalnessMap() ? mat.MetalnessMapPath : "";
		rvm.RoughnessMapPath = mat.HasRoughnessMap() ? mat.RoughnessMapPath : "";
		rvm.AoMapPath = mat.HasAoMap() ? mat.AoMapPath : "";

		return rvm;
	}

	#pragma endregion
	
};
