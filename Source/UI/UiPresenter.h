#pragma once

#include "ScenePane.h"
#include "PropsView/PropsPresenter.h"

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
};

class UiPresenter final : public ISceneViewDelegate
{
public:

	explicit UiPresenter(IUiPresenterDelegate& dgate, SceneManager& scene/*dependencies here*/)
	: _delegate(dgate), _scene(scene), _scenePane(ScenePane(this))
	{
		// TODO Set dependencies


		// Create views
		
		_propsPresenter = PropsPresenter();
	}

	~UiPresenter() = default;

	// Disable copy
	UiPresenter(const UiPresenter&) = delete;
	UiPresenter& operator=(const UiPresenter&) = delete;

	// Disable move
	UiPresenter(UiPresenter&&) = delete;
	UiPresenter& operator=(UiPresenter&&) = delete;

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
			ImGui::SetNextWindowPos(ImVec2(float(ScenePos().x), float(ScenePos().y)));
			ImGui::SetNextWindowSize(ImVec2(float(SceneSize().x), float(SceneSize().y)));

			auto& entsView = _scene.EntitiesView();
			std::vector<Entity*> allEnts{ entsView.size() };
			std::transform(entsView.begin(), entsView.end(), allEnts.begin(), [](const std::unique_ptr<Entity>& pe)
				{
					return pe.get();
				});
			//IblVm iblVm{ &_sceneController, &_renderOptions };
			_scenePane.DrawUI(allEnts, _selection/*, iblVm*/);


			// Properties Pane
			ImGui::SetNextWindowPos(ImVec2(float(PropsPos().x), float(PropsPos().y)));
			ImGui::SetNextWindowSize(ImVec2(float(PropsSize().x), float(PropsSize().y)));

			Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
			_propsPresenter.Draw((int)_selection.size(), selection, {}, {}/*, _gpuResourceService.get()*/);

		}
		ImGui::EndFrame();
		ImGui::Render();
	}

	
private:
	// Dependencies
	IUiPresenterDelegate& _delegate;
	SceneManager& _scene;
	
	// Views
	ScenePane _scenePane;
	PropsPresenter _propsPresenter;

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
		//_libraryController->LoadDemoScene();
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


		_scene.AddEntity(std::move(entity));
	}
	void CreateLight() override
	{
		printf("CreateLight()\n");
	}
	void CreateSphere() override
	{
		printf("CreateSphere()\n");
	}
	void CreateBlob() override
	{
		printf("CreateBlob()\n");
	}
	void CreateCube() override
	{
		printf("CreateCube()\n");
	}
	void DeleteSelected() override
	{
		printf("DeleteSelected()\n");
		std::vector<int> ids{};
		std::for_each(_selection.begin(), _selection.end(), [&ids](Entity* s) { ids.emplace_back(s->Id); });
		_delegate.Delete(ids);
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
		return 69.420f;
	}
	void SetExposure(float exposure) override
	{
		printf("SetExposure(%f)\n", exposure);
	}

	#pragma endregion
};
