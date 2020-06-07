
#include "SceneView.h"

#include <State/LibraryManager.h> //SkyboxInfo
#include <Framework/FileService.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h> // for ImGui::PushItemFlag() to enable disabling of widgets https://github.com/ocornut/imgui/issues/211

#include <unordered_set>
#include <string>


void SceneView::BuildUI(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection) const
{
	const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

	const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen;
	
	if (ImGui::Begin("LeftPane", nullptr, paneFlags))
	{
		SceneLoadPanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		CreationPanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		OutlinerPanel(ents, selection, headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();

		CameraPanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();
		
		IblPanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();

		BackdropPanel(headerFlags);
		ImGui::Spacing();
		ImGui::Spacing();

		PostPanel(headerFlags);
	}
	ImGui::End();
}

void SceneView::SceneLoadPanel(ImGuiTreeNodeFlags headerFlags) const
{
	// Scene Load Panel
	if (ImGui::CollapsingHeader("Load", headerFlags))
	{
		if (ImGui::BeginChild("Load", ImVec2{ 0,/*35*/50 }, true))
		{
			if (ImGui::Button("Object")) 
			{
				const auto path = FileService::ModelPicker();
				if (!path.empty()) { _del->LoadModel(path); }
			}
			
			if (ImGui::Button("Demo Scene")) { _del->LoadDemoScene(); }
			ImGui::SameLine();
			if (ImGui::Button("Heavy Demo Scene")) { _del->LoadHeavyDemoScene(); }
		}
		ImGui::EndChild();
	}
}

void SceneView::CreationPanel(ImGuiTreeNodeFlags headerFlags) const
{
	// Create Panel
	if (ImGui::CollapsingHeader("Create", headerFlags))
	{
		if (ImGui::BeginChild("Create##1", ImVec2{ 0,50 }, true))
		{
			if (ImGui::Button("Blob"))              { _del->CreateBlob(); }
			ImGui::SameLine();
			if (ImGui::Button("Sphere"))            { _del->CreateSphere(); }
			ImGui::SameLine();
			if (ImGui::Button("Cube"))              { _del->CreateCube(); }

			if (ImGui::Button("Point Light"))       { _del->CreatePointLight(); }
			ImGui::SameLine();
			if (ImGui::Button("Directional Light")) { _del->CreateDirectionalLight(); }
		}
		ImGui::EndChild();
	}
}

void SceneView::OutlinerPanel(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection, const ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Object List", headerFlags))
	{
		//ImGui::Text("Outliner");
		//ImGui::Text("Actions");
		
		if (ImGui::BeginChild("Scene_Outliner", ImVec2{ 0,170 }, true))
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

		if (ImGui::BeginChild("Scene_Actions", ImVec2{ 0,30 }, true))
		{
			// Delete selected button
			{
				if (selection.empty())
				{
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				if (ImGui::Button("Delete Selected")) _del->DeleteSelected();

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
				if (ImGui::Button("Delete All")) _del->DeleteAll();

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

void SceneView::IblPanel(ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Image Based Lighting", headerFlags))
	{
		auto roCopy = _del->GetRenderOptions();
		const auto& ibls = _del->GetSkyboxList();
		const auto activeIbl = _del->GetActiveSkybox();

		if (ImGui::BeginChild("IBL Panel", ImVec2{ 0,72 }, true))
		{
			if (ImGui::Button("Load")) {
				_del->LoadAndSetSkybox();
			}
			
			ImGui::SameLine();
			if (ImGui::BeginCombo("Map", ibls[activeIbl].Name.c_str()))
			{
				for (u32 idx = 0; idx < ibls.size(); ++idx)
				{
					const bool isSelected = idx == activeIbl;
					
					if (ImGui::Selectable(ibls[idx].Name.c_str(), isSelected)) {
						_del->SetActiveSkybox(idx);
					}
					
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::PushItemWidth(50);
			if (ImGui::DragFloat("Intensity", &roCopy.IblStrength, .01f, 0, 10, "%0.2f")) {
				_del->SetRenderOptions(roCopy);
			}
			ImGui::PopItemWidth();
			
			ImGui::PushItemWidth(50);
			if (ImGui::DragFloat("Rotation", &roCopy.SkyboxRotation, 1, 0, 0, "%.0f")) {
				_del->SetRenderOptions(roCopy);
			}
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}
}

void SceneView::BackdropPanel(ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Backdrop", headerFlags))
	{
		auto roCopy = _del->GetRenderOptions();

		if (ImGui::BeginChild("Backdrop Panel", ImVec2{ 0,51 }, true))
		{
			ImGui::PushItemWidth(50);
			if (ImGui::DragFloat("Brightness", &roCopy.BackdropBrightness, .01f, 0, 10, "%0.2f")) {
				_del->SetRenderOptions(roCopy);
			}
			ImGui::PopItemWidth();
			
			if (ImGui::Checkbox("Ambient Sky", &roCopy.ShowIrradiance)) _del->SetRenderOptions(roCopy);
		}
		ImGui::EndChild();
	}
}

void SceneView::CameraPanel(ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Camera", headerFlags))
	{
		if (ImGui::BeginChild("Camera Options", ImVec2{ 0,51 }, true))
		{
			auto roCopy = _del->GetRenderOptions();

			ImGui::PushItemWidth(50);
			if (ImGui::DragFloat("Exposure", &roCopy.ExposureBias, .01f, 0, 1000, "%0.2f")) { _del->SetRenderOptions(roCopy); }
			ImGui::PopItemWidth();

			if (ImGui::Checkbox("Show Clipping", &roCopy.ShowClipping)) { _del->SetRenderOptions(roCopy); }
		}
		ImGui::EndChild();
	}

}

void SceneView::PostPanel(ImGuiTreeNodeFlags headerFlags) const
{
	if (ImGui::CollapsingHeader("Post-Processing", headerFlags))
	{
		PostVignette();
		PostGrain();
	}
}

void SceneView::PostVignette() const
{
	const std::string id = "SceneView::PostPanel"; // id used to differentiate controls across all of imgui
	auto ro = _del->GetRenderOptions();
	auto& vo = ro.Vignette;
	const bool cachedEnable = vo.Enabled; // this is so the UI doesn't flicker when drawing the frame enabled changes
	const float height = vo.Enabled ? 103.f : 33.f;
	if (ImGui::BeginChild(("Vignette"+ id).c_str(), ImVec2{ 0, height }, true))
	{
		ImGui::Spacing();
			
		// Header
		{
			ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
			ImGui::Text("VIGNETTE");
			ImGui::PopStyleColor(1);

			// Enable button
			{
				bool enabled = (int)vo.Enabled;
				ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 17);
				if (ImGui::Checkbox(("##" + id).c_str(), &enabled))
				{
					vo.Enabled = (bool)enabled;
					_del->SetRenderOptions(ro);
				}
			}
		}
			
		ImGui::Spacing();

		if (cachedEnable) 
		{
			// Color
			{
				float* col = &vo.Color[0];
				if (ImGui::ColorEdit3(("Colour##" + id).c_str(), col)) {
					_del->SetRenderOptions(ro);
				}
			}
			

			// Inner Radius
			{
				float radius = vo.InnerRadius * 100;
				if (ImGui::SliderFloat(("Radius##" + id).c_str(), &radius, 0, 150, "%.0f")) 
				{
					const auto delta = (radius / 100) - vo.InnerRadius;
					vo.InnerRadius += delta;	
					vo.OuterRadius += delta;	
					_del->SetRenderOptions(ro);
				}
			}

				
			// Falloff
			{
				float falloff = (vo.OuterRadius - vo.InnerRadius) * 100;
				if (ImGui::SliderFloat(("Falloff##" + id).c_str(), &falloff, 0, 200, "%.0f")) 
				{
					vo.OuterRadius = vo.InnerRadius + (falloff / 100);	
					_del->SetRenderOptions(ro);
				}
			}
		}
	}
	ImGui::EndChild();
}

void SceneView::PostGrain() const
{
	const std::string id = "SceneView::PostPanel"; // id used to differentiate controls across all of imgui
	auto ro = _del->GetRenderOptions();
	auto& vo = ro.Vignette;
	const bool cachedEnable = vo.Enabled; // this is so the UI doesn't flicker when drawing the frame enabled changes
	const float height = vo.Enabled ? 103.f : 33.f;
	if (ImGui::BeginChild(("Grain"+ id).c_str(), ImVec2{ 0, height }, true))
	{
		ImGui::Spacing();
			
		// Header
		{
			ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
			ImGui::Text("VIGNETTE");
			ImGui::PopStyleColor(1);

			// Enable button
			{
				bool enabled = (int)vo.Enabled;
				ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 17);
				if (ImGui::Checkbox(("##" + id).c_str(), &enabled))
				{
					vo.Enabled = (bool)enabled;
					_del->SetRenderOptions(ro);
				}
			}
		}
			
		ImGui::Spacing();

		if (cachedEnable) 
		{
			// Color
			{
				float* col = &vo.Color[0];
				if (ImGui::ColorEdit3(("Colour##" + id).c_str(), col)) {
					_del->SetRenderOptions(ro);
				}
			}
			

			// Inner Radius
			{
				float radius = vo.InnerRadius * 100;
				if (ImGui::SliderFloat(("Radius##" + id).c_str(), &radius, 0, 150, "%.0f")) 
				{
					const auto delta = (radius / 100) - vo.InnerRadius;
					vo.InnerRadius += delta;	
					vo.OuterRadius += delta;	
					_del->SetRenderOptions(ro);
				}
			}

				
			// Falloff
			{
				float falloff = (vo.OuterRadius - vo.InnerRadius) * 100;
				if (ImGui::SliderFloat(("Falloff##" + id).c_str(), &falloff, 0, 200, "%.0f")) 
				{
					vo.OuterRadius = vo.InnerRadius + (falloff / 100);	
					_del->SetRenderOptions(ro);
				}
			}
		}
	}
	ImGui::EndChild();
}
