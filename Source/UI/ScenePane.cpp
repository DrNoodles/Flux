
#include "ScenePane.h"
#include "IblVm.h"

#include "Shared/FileService.h"
#include "App/Entity/Entity.h"

#include <imgui/imgui.h>

#include <unordered_set>
#include <string>


void ScenePane::DrawUI(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection/*, IblVm& iblVm*/) const
{
	assert(_delegate); // Delegate must be set
	
	const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

	if (ImGui::Begin("LeftPane", nullptr, paneFlags))
	{

		// Scene Load/Unload
		{
			ImGui::Text("Scene Loader");
			if (ImGui::BeginChild("Scene Loader", ImVec2{ 0,35 }, true))
			{
				if (ImGui::Button("Load Scene"))
				{
					_delegate->LoadDemoScene();
				}
				ImGui::SameLine();
				if (ImGui::Button("Load Object"))
				{
					const auto path = FileService::ModelPicker();
					if (!path.empty())
					{
						_delegate->LoadModel(path);
					}
				}
			}
			ImGui::EndChild();
			ImGui::Spacing();
		}

		
		// Creation
		{
			ImGui::Text("Create Primitives");
			if (ImGui::BeginChild("Create Primitives", ImVec2{ 0,60 }, true))
			{
				if (ImGui::Button("Sphere"))
				{
					_delegate->CreateSphere();
				}
				ImGui::SameLine();
				if (ImGui::Button("Blob"))
				{
					_delegate->CreateBlob();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cube"))
				{
					_delegate->CreateCube();
				}
				if (ImGui::Button("Light"))
				{
					_delegate->CreateLight();
				}
			}
			ImGui::EndChild();
			ImGui::Spacing();
		}
		

		
		
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



		// Scene Panel
		{
			if (ImGui::BeginChild("Scene Panel", ImVec2{ 0,200 }, true))
			{
				ImGui::Text("SCENE");
				for (Entity* e : ents)
				{
					const auto isSelected = IsSelected(e);
					if (ImGui::Selectable((e->Name + "##" + std::to_string(e->Id)).c_str(), isSelected))
					{
						// Deselect if already selected, otherwise update the selection.
						if (isSelected)
						{
							Deselect(e);
						}
						else
						{
							Select(e);
						}
					}
				}
			}
			ImGui::EndChild();
		}
		
		if (!selection.empty())
		{
			if (ImGui::Button("Delete")) _delegate->DeleteSelected();
			ImGui::SameLine();
		}

		if (ImGui::Button("Delete All")) _delegate->DeleteAll();


		ImGui::Spacing();

		
		//// IBL Panel
		//{
		//	auto& ibls = iblVm.Ibls;
		//	auto& activeIbl = iblVm.ActiveIbl;
		//	
		//	if (ImGui::BeginChild("IBL Panel", ImVec2{ 0,105 }, true))
		//	{
		//		ImGui::Text("IMAGE BASED LIGHTING");
		//		
		//		if (ImGui::BeginCombo("Map", ibls[activeIbl].c_str()))
		//		{
		//			for (int i = 0; i < (int)ibls.size(); ++i)
		//			{
		//				const bool isSelected = i == activeIbl;
		//				if (ImGui::Selectable(ibls[i].c_str(), isSelected))
		//				{
		//					activeIbl = i;
		//				}
		//				if (isSelected)
		//				{
		//					ImGui::SetItemDefaultFocus();
		//				}
		//			}
		//			ImGui::EndCombo();
		//		}
		//		ImGui::PushItemWidth(40);
		//		if (ImGui::DragFloat("Rotation", &iblVm.Rotation, 1, 0, 0, "%0.f")) iblVm.Commit();
		//		ImGui::PopItemWidth();

		//		if (ImGui::Checkbox("Show Irradiance", &iblVm.ShowIrradiance)) iblVm.Commit();
		//	}
		//	ImGui::EndChild();
		//	ImGui::Spacing();
		//}

		float exposure = _delegate->GetExposure();
		if (ImGui::DragFloat("Exposure", &exposure, .01f, 0, 1000, "%0.2f")) _delegate->SetExposure(exposure);
	}
	ImGui::End();
}
