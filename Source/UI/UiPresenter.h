#pragma once

#include "ScenePane.h"
#include "PropsView/PropsPresenter.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

class IUiPresenterDelegate
{
public:
	virtual ~IUiPresenterDelegate() = default;
	virtual glm::ivec2 GetWindowSize() const = 0;
};

class UiPresenter final : public ISceneViewDelegate
{
public:

	explicit UiPresenter(IUiPresenterDelegate& dgate /*dependencies here*/)
	: _dgate(dgate), _scenePane(ScenePane(this))
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

			/*auto& entsView = _sceneController.EntitiesView();
			std::vector<Entity*> allEnts{ entsView.size() };
			std::transform(entsView.begin(), entsView.end(), allEnts.begin(), [](const std::unique_ptr<Entity>& pe)
				{
					return pe.get();
				});*/
			//IblVm iblVm{ &_sceneController, &_renderOptions };
			auto _selection = std::unordered_set<Entity*>();
			auto allEnts = std::vector<Entity*>();
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
	IUiPresenterDelegate& _dgate;

	// Views
	ScenePane _scenePane;
	PropsPresenter _propsPresenter;

	// Layout
	int _scenePanelWidth = 250;
	int _propsPanelWidth = 300;
	

	int WindowWidth() const { return _dgate.GetWindowSize().x; }
	int WindowHeight() const { return _dgate.GetWindowSize().y; }
	
	// Anchor left
	glm::ivec2 ScenePos() const { return { 0, 0 }; }
	glm::ivec2 SceneSize() const
	{
		int i = _dgate.GetWindowSize().y;
		return { _scenePanelWidth, i };
	}

	// Fit to middle
	glm::ivec2 ViewportPos() const { return { _scenePanelWidth, 0 }; }
	glm::ivec2 ViewportSize() const { return { WindowWidth() - _propsPanelWidth - _scenePanelWidth, WindowHeight() }; }
	// Anchor right
	glm::ivec2 PropsPos() const { return { WindowWidth() - _propsPanelWidth,0 }; }
	glm::ivec2 PropsSize() const { return { _propsPanelWidth, WindowHeight() }; }



	#pragma region ISceneViewDelegate
	void LoadDemoScene() override
	{
		printf("LoadDemoScene()");
	}
	void LoadModel(const std::string& path) override
	{
		printf("LoadModel(%s)", path.c_str());
	}
	void CreateLight() override
	{
		printf("CreateLight()");
	}
	void CreateSphere() override
	{
		printf("CreateSphere()");
	}
	void CreateBlob() override
	{
		printf("CreateBlob()");
	}
	void CreateCube() override
	{
		printf("CreateCube()");
	}
	void DeleteSelected() override
	{
		printf("DeleteSelected()");
	}
	void DeleteAll() override
	{
		printf("DeleteAll()");
	}
	float GetExposure() const override
	{
		//printf("GetExposure()");
		return 69.420f;
	}
	void SetExposure(float exposure) override
	{
		printf("SetExposure(%f)", exposure);
	}
	#pragma endregion 
};
