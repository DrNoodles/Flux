#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

class UiPresenter
{
public:
	UiPresenter(/*dependencies here*/)
	{
		// TODO Set dependencies


		// TODO 

		
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
			auto show_demo_window = true;
			ImGui::ShowDemoWindow(&show_demo_window);
			//
			//// Scene Pane
			//ImGui::SetNextWindowPos(ImVec2(float(ScenePos().x), float(ScenePos().y)));
			//ImGui::SetNextWindowSize(ImVec2(float(SceneSize().x), float(SceneSize().y)));

			//auto& entsView = _sceneController.EntitiesView();
			//std::vector<Entity*> allEnts{ entsView.size() };
			//std::transform(entsView.begin(), entsView.end(), allEnts.begin(), [](const std::unique_ptr<Entity>& pe)
			//	{
			//		return pe.get();
			//	});
			//IblVm iblVm{ &_sceneController, &_renderOptions };
			//_scenePane.DrawUI(allEnts, _selection, iblVm);


			//// Properties Pane
			//ImGui::SetNextWindowPos(ImVec2(float(PropsPos().x), float(PropsPos().y)));
			//ImGui::SetNextWindowSize(ImVec2(float(PropsSize().x), float(PropsSize().y)));

			//Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
			//_propsPresenter.Draw((int)_selection.size(), selection, _textures, _meshes, _gpuResourceService.get());

		}
		ImGui::EndFrame();
		ImGui::Render();
	}
private:
	
};