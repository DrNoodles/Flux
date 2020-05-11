
#include "SceneView.h"
#include "IblVm.h"

#include <State/LibraryManager.h> //SkyboxInfo
#include <Framework/FileService.h>
#include <Renderer/Renderer.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h> // for ImGui::PushItemFlag() to enable disabling of widgets https://github.com/ocornut/imgui/issues/211

#include <unordered_set>
#include <string>


void SceneView::BuildUI(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection, IblVm& iblVm) const
{
	assert(_delegate); // Delegate must be set
	
	const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

	const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen;
	
	if (ImGui::Begin("LeftPane", nullptr, paneFlags))
	{
		SceneLoadPanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		CreatePanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		OutlinerPanel(ents, selection, headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		IblPanel(iblVm, headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		RenderOptionsPanel(headerFlags);
	}
	ImGui::End();
}

void SceneView::SceneLoadPanel(ImGuiTreeNodeFlags headerFlags) const
{
	// Scene Load Panel
	if (ImGui::CollapsingHeader("Load", headerFlags))
	{
		if (ImGui::BeginChild("Load", ImVec2{ 0,/*35*/60 }, true))
		{
			if (ImGui::Button("Demo Scene"))
			{
				_delegate->LoadDemoScene();
			}
			ImGui::SameLine();
			if (ImGui::Button("Object"))
			{
				const auto path = FileService::ModelPicker();
				if (!path.empty())
				{
					_delegate->LoadModel(path);
				}
			}

			if (ImGui::Button("Heavy Demo Scene"))
			{
				_delegate->LoadHeavyDemoScene();
			}
		}
		ImGui::EndChild();
	}
}

void SceneView::CreatePanel(ImGuiTreeNodeFlags headerFlags) const
{
	// Create Panel
	if (ImGui::CollapsingHeader("Create", headerFlags))
	{
		if (ImGui::BeginChild("Create##1", ImVec2{ 0,60 }, true))
		{
			if (ImGui::Button("Blob"))
			{
				_delegate->CreateBlob();
			}
			ImGui::SameLine();
			if (ImGui::Button("Sphere"))
			{
				_delegate->CreateSphere();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cube"))
			{
				_delegate->CreateCube();
			}


			if (ImGui::Button("Point Light"))
			{
				_delegate->CreatePointLight();
			}
			ImGui::SameLine();
			if (ImGui::Button("Directional Light"))
			{
				_delegate->CreateDirectionalLight();
			}
		}
		ImGui::EndChild();
	}
}

void SceneView::OutlinerPanel(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection, const ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Outliner", headerFlags))
	{
		ImGui::Text("Outliner");
		if (ImGui::BeginChild("Scene_Outliner", ImVec2{ 0,200 }, true))
		{

			// Selection helper lambdas
			const auto IsSelected = [&selection](Entity* target)
			{
				return selection.find(target) != selection.end();
			};
			const auto Select = [&selection](Entity* target)
			{
				selection.clear(); // only allowing single selection
				selection.insert(target);
			};
			const auto Deselect = [&selection](Entity* target)
			{
				selection.erase(target);
			};


			for (Entity* e : ents)
			{
				const auto isSelected = IsSelected(e);
				if (ImGui::Selectable((e->Name + "##" + std::to_string(e->Id)).c_str(), isSelected))
				{
					// Deselect if already selected, otherwise update the selection.
					if (isSelected)
					{
						Deselect(e);
						printf_s("Deselected %s\n", e->Name.c_str());
					}
					else
					{
						Select(e);
						printf_s("Selected %s\n", e->Name.c_str());
					}
				}
			}
		}
		ImGui::EndChild();
		ImGui::Spacing();


		ImGui::Text("Actions");
		if (ImGui::BeginChild("Scene_Actions", ImVec2{ 0,35 }, true))
		{
			// Delete selected button
			{
				if (selection.empty())
				{
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				if (ImGui::Button("Delete Selected")) _delegate->DeleteSelected();

				if (selection.empty())
				{
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}

			// Delete All button
			{
				if (ents.empty())
				{
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				ImGui::SameLine();
				if (ImGui::Button("Delete All")) _delegate->DeleteAll();

				if (ents.empty())
				{
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}
		}
		ImGui::EndChild();
	}
}

void SceneView::IblPanel(IblVm& iblVm, ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Image Based Lighting", headerFlags))
	{
		auto roCopy = _delegate->GetRenderOptions();
		auto& ibls = _delegate->GetSkyboxList();
		const auto activeIbl = _delegate->GetActiveSkybox();

		if (ImGui::BeginChild("IBL Panel", ImVec2{ 0,130 }, true))
		{
			if (ImGui::Button("Load Skybox"))
			{
				_delegate->LoadSkybox();
			}

			if (ImGui::BeginCombo("Map", ibls[activeIbl].Name.c_str()))
			{
				for (u32 idx = 0; idx < ibls.size(); ++idx)
				{
					const bool isSelected = idx == activeIbl;
					if (ImGui::Selectable(ibls[idx].Name.c_str(), isSelected))
					{
						_delegate->SetActiveSkybox(idx);
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::PushItemWidth(50);
			if (ImGui::DragFloat("Strength", &roCopy.IblStrength, .01f, 0, 10, "%0.2f")) {
				_delegate->SetRenderOptions(roCopy);
			}
			ImGui::PopItemWidth();
			
			ImGui::PushItemWidth(50);
			if (ImGui::DragInt("Rotation", &iblVm.Rotation, 1, 0, 0)) iblVm.Commit();
			ImGui::PopItemWidth();

			if (ImGui::Checkbox("Show Irradiance", &iblVm.ShowIrradiance)) iblVm.Commit();
		}
		ImGui::EndChild();
	}

}

/*void SceneView::AnimatePanel(ImGuiTreeNodeFlags headerFlags) const
{
}*/
void SceneView::RenderOptionsPanel(ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Render Options", headerFlags))
	{
		if (ImGui::BeginChild("Render Options all", ImVec2{ 0,60 }, true))
		{
			auto roCopy = _delegate->GetRenderOptions();

			ImGui::PushItemWidth(50);
			if (ImGui::DragFloat("Exposure", &roCopy.ExposureBias, .01f, 0, 1000, "%0.2f")) {
				_delegate->SetRenderOptions(roCopy);
			}
			ImGui::PopItemWidth();

			if (ImGui::Checkbox("Show Clipping", &roCopy.ShowClipping)) {
				_delegate->SetRenderOptions(roCopy);
			}
		}
		ImGui::EndChild();
	}

}
