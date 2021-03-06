
#include "PropsView.h"

#include "MaterialViewState.h"
#include "TransformVm.h"
#include "LightVm.h"

#include <Framework/FileService.h>

#include <imgui/imgui.h>
#include <glm/common.hpp>



// Helper function
std::string FormatMapPath(const std::string& path, int limit = 30)
{
	// TODO Test that limit trimming doesn't overflow
	return (path.size() > limit)
		? "..." + path.substr(path.size() - limit + 3)
		: path;
}

const ImGuiWindowFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen;

void PropsView::BuildUI(int selectionCount, 
	TransformVm& tvm,
	std::optional<LightVm>& lvm) const
{
	const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoCollapse;
	
	if (ImGui::Begin("RightPanel", nullptr, paneFlags))
	{
		// Transform & Lighting Panels
		if (selectionCount == 1)
		{
			DrawTransformPanel(tvm);
		
			if (lvm.has_value())
			{
				ImGui::Spacing();
				ImGui::Spacing();
				DrawLightPanel(lvm.value());
			}
		}
	
		// Material Panel
		ImGui::Spacing();
		ImGui::Spacing();
		auto mvm = _delegate->GetMaterialState();
		DrawMaterialPanel(mvm);
	}
	ImGui::End();
}


void PropsView::DrawTransformPanel(TransformVm& tvm) const
{
	// TODO This gets Commit()'d every frame we're mid edit and the value is changed by the system
	// Desired behaviour is if hitting escape the value should be untouched
	
	tvm.Refresh();
	if (ImGui::CollapsingHeader("Transform", headerFlags))
	{
		ImGui::Spacing();
		ImGui::PushItemWidth(170);
		if (ImGui::DragFloat3("Position", &tvm.Pos[0], 0.1f)) tvm.Commit();
		if (ImGui::DragFloat3("Rotation", &tvm.Rot[0])) tvm.Commit();
		if (ImGui::DragFloat3("Scale", &tvm.Scale[0], 0.1f)) tvm.Commit();
		ImGui::PopItemWidth();
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
		ImGui::SetNextItemWidth(40);
		ImGui::Checkbox("Lock", &tvm.UniformScale);
	}
}


void PropsView::DrawLightPanel(LightVm& lvm) const
{
	if (ImGui::CollapsingHeader("Light", headerFlags))
	{
		ImGui::Spacing();
		if (ImGui::ColorEdit3("Color##Light", &lvm.Color[0])) lvm.CommitChanges();
		if (ImGui::DragFloat("Intensity##Light", &lvm.Intensity, 10, 0, 1000000, "%.3f", 2)) lvm.CommitChanges();
	}
}


void SubSectionSpacing()
{
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
}

float modeStart = 78;
float buttonRight = 35;

void PropsView::DrawMaterialPanel(std::optional<MaterialViewState>& mvm) const
{
	if (ImGui::CollapsingHeader("Material", headerFlags))
	{
		ImGui::Spacing();
		

		ImGui::Text("Materials");
		const auto& materials = _delegate->GetMaterials();
		const auto height = glm::min<f32>(5.f, f32(materials.size()) + 1) * ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("Materials", ImVec2{ 0,height }, true))
		{
			for (i32 i = 0; i < materials.size(); ++i)
			{
				const auto& [name,_] = materials[i];

				if (ImGui::Selectable((name + "##" + std::to_string(i)).c_str(), i == _delegate->GetSelectedMaterial()))
				{
					_delegate->SelectMaterial(i);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(name.c_str());
			}
		}
		ImGui::EndChild();


		ImGui::Spacing();
		ImGui::Spacing();


		if (mvm.has_value())
		{
			ImGui::Text("Material");
		
			int soloSelection = (int)mvm->ActiveSolo;
			// NOTE: The order must match TextureType
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130);
			ImGui::SetNextItemWidth(100);
			if (ImGui::Combo("Solo Texture", &soloSelection,
				"All\0Base Color\0Normals\0Metalness\0Roughness\0AO\0Emissive\0Transparency\0"))
			{
				mvm->ActiveSolo = soloSelection;
				_delegate->CommitMaterialChanges(*mvm);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Display only the selected texture.");

			if (ImGui::BeginChild("Material Panel", ImVec2{ 0,0 }, true))
			{
				ImGui::Spacing();
				Basecolor(*mvm);
				SubSectionSpacing();

				Normals(*mvm);
				SubSectionSpacing();

				Metalness(*mvm);
				SubSectionSpacing();

				Roughness(*mvm);
				SubSectionSpacing();

				AmbientOcclusion(*mvm);
				SubSectionSpacing();

				Emissive(*mvm);
				SubSectionSpacing();

				Transparency(*mvm);
			}
			ImGui::EndChild();
		}
	}
}

void PropsView::Basecolor(MaterialViewState& rvm) const
{
	bool& useMap = rvm.UseBasecolorMap;
	std::string& mapPath = rvm.BasecolorMapPath;
	const std::string& valueName = "Base Color";
	float* col = &rvm.Basecolor[0];
	

	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text("BASE COLOR");
	ImGui::PopStyleColor(1);

	
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - modeStart);
	ImGui::PushItemWidth(80);
	int current = useMap;
	if (ImGui::Combo(("##" + valueName).c_str(), &current, "Value\0Texture\0"))
	{
		useMap = current;
		_delegate->CommitMaterialChanges(rvm);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(("Define " + valueName + " via a uniform value or texture").c_str());
	ImGui::PopItemWidth();

	
	ImGui::Spacing();


	if (useMap)
	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				_delegate->CommitMaterialChanges(rvm);
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine();
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			_delegate->CommitMaterialChanges(rvm);
		}
	}
	else
	{
		if (ImGui::ColorEdit3((valueName + "##" + valueName).c_str(), col)) _delegate->CommitMaterialChanges(rvm);
	}
}

void PropsView::Normals(MaterialViewState& rvm) const
{
	const std::string title = "NORMALS";
	const std::string valueName = "Normals";
	//bool& useMap = rvm.UseNormalMap;
	std::string& mapPath = rvm.NormalMapPath;
	bool& invertY = rvm.InvertNormalMapY;
	bool& invertZ = rvm.InvertNormalMapZ;


	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);


	ImGui::Spacing();


	const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

	if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
	{
		const auto newPath = FileService::TexturePicker();
		if (!newPath.empty())
		{
			mapPath = newPath;
			_delegate->CommitMaterialChanges(rvm);
		}
	}
	if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

	ImGui::SameLine();
	if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
	{
		mapPath = "";
		_delegate->CommitMaterialChanges(rvm);
	}

	ImGui::Spacing();

	if (ImGui::Checkbox(("Invert Y##" + valueName).c_str(), &invertY)) _delegate->CommitMaterialChanges(rvm);
	ImGui::SameLine();
	if (ImGui::Checkbox(("Invert Z##" + valueName).c_str(), &invertZ)) _delegate->CommitMaterialChanges(rvm);
}

void PropsView::Metalness(MaterialViewState& rvm) const
{
	const std::string title = "METALNESS";
	const std::string valueName = "Metalness";
	bool& useMap = rvm.UseMetalnessMap;
	std::string& mapPath = rvm.MetalnessMapPath;
	bool& invertMap = rvm.InvertMetalnessMap;
	int& activeChannel = rvm.ActiveMetalnessChannel;
	float& value = rvm.Metalness;
	
	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);

	
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - modeStart);
	ImGui::PushItemWidth(80);
	int current = useMap;
	if (ImGui::Combo(("##" + valueName).c_str(), &current, "Value\0Texture\0"))
	{
		useMap = current;
		_delegate->CommitMaterialChanges(rvm);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(("Define " + valueName + " via a uniform value or texture").c_str());
	ImGui::PopItemWidth();

	
	ImGui::Spacing();


	if (useMap)
	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);
		
		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				_delegate->CommitMaterialChanges(rvm);
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine();
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			_delegate->CommitMaterialChanges(rvm);
		}

		ImGui::Spacing();

		ImGui::PushItemWidth(70);
		if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), MaterialViewState::MapChannels[activeChannel].c_str()))
		{
			for (int i = 0; i < (int)MaterialViewState::MapChannels.size(); ++i)
			{
				const bool isSelected = i == activeChannel;
				if (ImGui::Selectable(MaterialViewState::MapChannels[i].c_str(), isSelected))
				{
					activeChannel = i;
					_delegate->CommitMaterialChanges(rvm);
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine(160);
		if (ImGui::Checkbox(("Invert##" + valueName).c_str(), &invertMap)) _delegate->CommitMaterialChanges(rvm);
	}
	else
	{
		if (ImGui::SliderFloat((valueName + "##" + valueName).c_str(), &value, 0, 1)) _delegate->CommitMaterialChanges(rvm);
	}
}

void PropsView::Roughness(MaterialViewState& rvm) const
{
	const std::string title = "ROUGHNESS";
	const std::string valueName = "Roughness";
	bool& useMap = rvm.UseRoughnessMap;
	std::string& mapPath = rvm.RoughnessMapPath;
	bool& invertMap = rvm.InvertRoughnessMap;
	int& activeChannel = rvm.ActiveRoughnessChannel;
	float& value = rvm.Roughness;

	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);

	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - modeStart);
	ImGui::PushItemWidth(80);
	int current = useMap;
	if (ImGui::Combo(("##" + valueName).c_str(), &current, "Value\0Texture\0"))
	{
		useMap = current;
		_delegate->CommitMaterialChanges(rvm);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(("Define " + valueName + " via a uniform value or texture").c_str());
	ImGui::PopItemWidth();

	ImGui::Spacing();


	if (useMap)
	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				_delegate->CommitMaterialChanges(rvm);
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine();
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			_delegate->CommitMaterialChanges(rvm);
		}

		ImGui::Spacing();

		ImGui::PushItemWidth(70);
		if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), MaterialViewState::MapChannels[activeChannel].c_str()))
		{
			for (int i = 0; i < (int)MaterialViewState::MapChannels.size(); ++i)
			{
				const bool isSelected = i == activeChannel;
				if (ImGui::Selectable(MaterialViewState::MapChannels[i].c_str(), isSelected))
				{
					activeChannel = i;
					_delegate->CommitMaterialChanges(rvm);
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine(160);
		if (ImGui::Checkbox(("Invert##" + valueName).c_str(), &invertMap)) _delegate->CommitMaterialChanges(rvm);
	}
	else
	{
		if (ImGui::SliderFloat((valueName + "##" + valueName).c_str(), &value, 0, 1)) _delegate->CommitMaterialChanges(rvm);
	}
}

void PropsView::AmbientOcclusion(MaterialViewState& rvm) const
{
	const std::string title = "AMBIENT OCCLUSION";
	const std::string valueName = "AO";
	//bool& useMap = rvm.UseAoMap;
	std::string& mapPath = rvm.AoMapPath;
	bool& invertMap = rvm.InvertAoMap;
	int& activeChannel = rvm.ActiveAoChannel;

	
	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);


	ImGui::Spacing();


	const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

	if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
	{
		const auto newPath = FileService::TexturePicker();
		if (!newPath.empty())
		{
			mapPath = newPath;
			_delegate->CommitMaterialChanges(rvm);
		}
	}
	if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

	ImGui::SameLine();
	if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
	{
		mapPath = "";
		_delegate->CommitMaterialChanges(rvm);
	}

	ImGui::Spacing();

	ImGui::PushItemWidth(70);
	if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), MaterialViewState::MapChannels[activeChannel].c_str()))
	{
		for (int i = 0; i < (int)MaterialViewState::MapChannels.size(); ++i)
		{
			const bool isSelected = i == activeChannel;
			if (ImGui::Selectable(MaterialViewState::MapChannels[i].c_str(), isSelected))
			{
				activeChannel = i;
				_delegate->CommitMaterialChanges(rvm);
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	ImGui::SameLine(160);
	if (ImGui::Checkbox(("Invert##" + valueName).c_str(), &invertMap)) _delegate->CommitMaterialChanges(rvm);
}

void PropsView::Emissive(MaterialViewState& rvm) const
{
	const std::string title = "EMISSIVE";
	const std::string valueName = "Intensity";
	std::string& mapPath = rvm.EmissiveMapPath;
	float& value = rvm.EmissiveIntensity;


	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);


	ImGui::Spacing();


	const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

	if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
	{
		const auto newPath = FileService::TexturePicker();
		if (!newPath.empty())
		{
			mapPath = newPath;
			_delegate->CommitMaterialChanges(rvm);
		}
	}
	if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

	ImGui::SameLine();
	if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
	{
		mapPath = "";
		_delegate->CommitMaterialChanges(rvm);
	}

	ImGui::Spacing();

	if (ImGui::SliderFloat((valueName + "##" + valueName).c_str(), &value, 0, 20, "%.2f", 2)) 
		_delegate->CommitMaterialChanges(rvm);
}

void PropsView::Transparency(MaterialViewState& rvm) const
{
	const std::string title = "TRANSPARENCY";
	const std::string valueName = "Threshold";
	std::string& mapPath = rvm.TransparencyMapPath;
	int& activeChannel = rvm.ActiveTransparencyChannel;
	float& value = rvm.TransparencyCutoffThreshold;

	{
		ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
		ImGui::Text(title.c_str());
		ImGui::PopStyleColor(1);
	}


	{
		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 88);
		ImGui::PushItemWidth(90);
		if (ImGui::Combo(("##mode" + valueName).c_str(), &rvm.TransparencyMode, "Additive\0Cutoff\0"))
		{
			_delegate->CommitMaterialChanges(rvm);
		}

		const std::string tip = "\
Set the blending mode\n\
  Additive: linearly blends from transparent (black) to opaque (white)\n\
  Cutoff: is transparent for all values below the threshold";
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(tip.c_str());
		ImGui::PopItemWidth();
	}

	ImGui::Spacing();

	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);
		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x - buttonRight, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				_delegate->CommitMaterialChanges(rvm);
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine();
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			_delegate->CommitMaterialChanges(rvm);
		}
	}
	
	ImGui::Spacing();

	{
		ImGui::PushItemWidth(70);
		if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), MaterialViewState::MapChannels[activeChannel].c_str()))
		{
			for (int i = 0; i < (int)MaterialViewState::MapChannels.size(); ++i)
			{
				const bool isSelected = i == activeChannel;
				if (ImGui::Selectable(MaterialViewState::MapChannels[i].c_str(), isSelected))
				{
					activeChannel = i;
					_delegate->CommitMaterialChanges(rvm);
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}
	
	ImGui::Spacing();

	if (rvm.TransparencyMode == 1 && ImGui::SliderFloat((valueName + "##" + valueName).c_str(), &value, 0, 1, "%.2f", 1)) {
		_delegate->CommitMaterialChanges(rvm);
	}
}
