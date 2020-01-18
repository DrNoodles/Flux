
#include "PropsView.h"

#include "RenderableVm.h"
#include "TransformVm.h"
#include "LightVm.h"
#include "Shared/FileService.h"

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

void PropsView::DrawUI(int selectionCount, 
	TransformVm& tvm,
	std::optional<RenderableVm>& rvm,
	std::optional<LightVm>& lvm) const
{
	const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoCollapse;
	
	if (ImGui::Begin("RightPanel", nullptr, paneFlags))
	{
		// Empty state
		if (selectionCount != 1)
		{
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
			ImGui::Text(selectionCount == 0 ? "No Selection" : "Multi-Selection");
			ImGui::End();
			return;
		}

		DrawTransformPanel(tvm);
		if (lvm.has_value())
		{
			ImGui::Spacing();
			ImGui::Spacing();
			DrawLightPanel(lvm.value());
		}
		if (rvm.has_value())
		{
			ImGui::Spacing();
			ImGui::Spacing();
			//DrawRenderablePanel(rvm.value());
		}



		
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
		ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - 45);
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
		if (ImGui::DragFloat("Intensity##Light", &lvm.Intensity, 10, 0, 1000000)) lvm.CommitChanges();
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
/*
void PropsView::DrawRenderablePanel(RenderableVm& rvm) const
{
	if (ImGui::CollapsingHeader("Model", headerFlags))
	{
		ImGui::Spacing();


		ImGui::Text("Sub-Mesh");
		const auto height = glm::min(5, int(rvm.MeshVms.size()) + 1) * ImGui::GetItemsLineHeightWithSpacing();
		if (ImGui::BeginChild("Sub-Mesh Panel", ImVec2{ 0,height }, true))
		{
			for (int i = 0; i < rvm.MeshVms.size(); ++i)
			{
				const auto& mesh = rvm.MeshVms[i];

				if (ImGui::Selectable((mesh + "##" + mesh).c_str(), i == rvm.GetSelectedSubMesh()))
				{
					rvm.SelectSubMesh(i);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(mesh.c_str());
			}
		}
		ImGui::EndChild();


		ImGui::Spacing();
		ImGui::Spacing();


		ImGui::Text("Material");
		
		int soloSelection = (int)rvm.ActiveSolo;
		// NOTE: The order must match TextureType
		ImGui::SameLine(ImGui::GetContentRegionAvailWidth()-125);
		ImGui::SetNextItemWidth(100);
		if (ImGui::Combo("Solo Texture", &soloSelection, "All\0Base Color\0Metalness\0Roughness\0AO\0Normals"))
		{
			rvm.ActiveSolo = (TextureType)soloSelection;
			rvm.CommitChanges();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Display only the selected texture.");

		if (ImGui::BeginChild("Material Panel", ImVec2{ 0,0 }, true))
		{
			ImGui::Spacing();
			BaseColor(rvm);
			SubSectionSpacing();

			Metalness(rvm);
			SubSectionSpacing();

			Roughness(rvm);
			SubSectionSpacing();

			AmbientOcclusion(rvm);
			SubSectionSpacing();

			Normals(rvm);
		}
		ImGui::EndChild();
	}
}

void PropsView::BaseColor(RenderableVm& rvm) const
{
	bool& useMap = rvm.UseBaseColorMap;
	std::string& mapPath = rvm.BaseColorMapPath;
	const std::string& valueName = "Base Color";
	float* col = &rvm.BaseColor[0];
	

	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text("BASE COLOR");
	ImGui::PopStyleColor(1);

	
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 70);
	ImGui::PushItemWidth(80);
	int current = useMap;
	if (ImGui::Combo(("##" + valueName).c_str(), &current, "Value\0Texture"))
	{
		useMap = current;
		rvm.CommitChanges();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(("Define " + valueName + " via a uniform value or texture").c_str());
	ImGui::PopItemWidth();

	
	ImGui::Spacing();


	if (useMap)
	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvailWidth() - 35, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				rvm.CommitChanges();
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 20);
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			rvm.CommitChanges();
		}
	}
	else
	{
		if (ImGui::ColorEdit3((valueName + "##" + valueName).c_str(), col)) rvm.CommitChanges();
	}
}

void PropsView::Metalness(RenderableVm& rvm) const
{
	const std::string title = "METALLIC";
	const std::string valueName = "Metalness";
	bool& useMap = rvm.UseMetalnessMap;
	std::string& mapPath = rvm.MetalnessMapPath;
	bool& invertMap = rvm.InvertMetalnessMap;
	int& activeChannel = rvm.ActiveMetalnessChannel;
	float& value = rvm.Metalness;
	
	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);

	
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 70);
	ImGui::PushItemWidth(80);
	int current = useMap;
	if (ImGui::Combo(("##" + valueName).c_str(), &current, "Value\0Texture"))
	{
		useMap = current;
		rvm.CommitChanges();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(("Define " + valueName + " via a uniform value or texture").c_str());
	ImGui::PopItemWidth();

	
	ImGui::Spacing();


	if (useMap)
	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);
		
		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvailWidth() - 35, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				rvm.CommitChanges();
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 20);
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			rvm.CommitChanges();
		}

		ImGui::Spacing();

		ImGui::PushItemWidth(70);
		if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), RenderableVm::MapChannels[activeChannel].c_str()))
		{
			for (int i = 0; i < (int)RenderableVm::MapChannels.size(); ++i)
			{
				const bool isSelected = i == activeChannel;
				if (ImGui::Selectable(RenderableVm::MapChannels[i].c_str(), isSelected))
				{
					activeChannel = i;
					rvm.CommitChanges();
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
		if (ImGui::Checkbox(("Invert##" + valueName).c_str(), &invertMap)) rvm.CommitChanges();
	}
	else
	{
		if (ImGui::SliderFloat((valueName + "##" + valueName).c_str(), &value, 0, 1)) rvm.CommitChanges();
	}
}

void PropsView::Roughness(RenderableVm& rvm) const
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

	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 70);
	ImGui::PushItemWidth(80);
	int current = useMap;
	if (ImGui::Combo(("##" + valueName).c_str(), &current, "Value\0Texture"))
	{
		useMap = current;
		rvm.CommitChanges();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(("Define " + valueName + " via a uniform value or texture").c_str());
	ImGui::PopItemWidth();

	ImGui::Spacing();


	if (useMap)
	{
		const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

		if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvailWidth() - 35, 0 }))
		{
			const auto newPath = FileService::TexturePicker();
			if (!newPath.empty())
			{
				mapPath = newPath;
				rvm.CommitChanges();
			}
		}
		if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 20);
		if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
		{
			mapPath = "";
			rvm.CommitChanges();
		}

		ImGui::Spacing();

		ImGui::PushItemWidth(70);
		if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), RenderableVm::MapChannels[activeChannel].c_str()))
		{
			for (int i = 0; i < (int)RenderableVm::MapChannels.size(); ++i)
			{
				const bool isSelected = i == activeChannel;
				if (ImGui::Selectable(RenderableVm::MapChannels[i].c_str(), isSelected))
				{
					activeChannel = i;
					rvm.CommitChanges();
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
		if (ImGui::Checkbox(("Invert##" + valueName).c_str(), &invertMap)) rvm.CommitChanges();
	}
	else
	{
		if (ImGui::SliderFloat((valueName + "##" + valueName).c_str(), &value, 0, 1)) rvm.CommitChanges();
	}
}

void PropsView::AmbientOcclusion(RenderableVm& rvm) const
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

	if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvailWidth() - 35, 0 }))
	{
		const auto newPath = FileService::TexturePicker();
		if (!newPath.empty())
		{
			mapPath = newPath;
			rvm.CommitChanges();
		}
	}
	if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 20);
	if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
	{
		mapPath = "";
		rvm.CommitChanges();
	}

	ImGui::Spacing();

	ImGui::PushItemWidth(70);
	if (ImGui::BeginCombo(("Channel##" + valueName).c_str(), RenderableVm::MapChannels[activeChannel].c_str()))
	{
		for (int i = 0; i < (int)RenderableVm::MapChannels.size(); ++i)
		{
			const bool isSelected = i == activeChannel;
			if (ImGui::Selectable(RenderableVm::MapChannels[i].c_str(), isSelected))
			{
				activeChannel = i;
				rvm.CommitChanges();
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
	if (ImGui::Checkbox(("Invert##" + valueName).c_str(), &invertMap)) rvm.CommitChanges();
}

void PropsView::Normals(RenderableVm& rvm) const
{
	const std::string title = "NORMALS";
	const std::string valueName = "Normals";
	//bool& useMap = rvm.UseNormalMap;
	std::string& mapPath = rvm.NormalMapPath;
	bool& invertMap = rvm.InvertNormalMapZ;


	ImGui::PushStyleColor(ImGuiCol_Text, _headingColor);
	ImGui::Text(title.c_str());
	ImGui::PopStyleColor(1);


	ImGui::Spacing();


	const std::string btnText = mapPath.empty() ? "Browse..." : FormatMapPath(mapPath);

	if (ImGui::Button((btnText + "##" + valueName).c_str(), ImVec2{ ImGui::GetContentRegionAvailWidth() - 35, 0 }))
	{
		const auto newPath = FileService::TexturePicker();
		if (!newPath.empty())
		{
			mapPath = newPath;
			rvm.CommitChanges();
		}
	}
	if (ImGui::IsItemHovered() && !mapPath.empty()) ImGui::SetTooltip(mapPath.c_str());

	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 20);
	if (ImGui::Button(("X##" + valueName).c_str(), ImVec2{ 30,0 }))
	{
		mapPath = "";
		rvm.CommitChanges();
	}

	ImGui::Spacing();

	if (ImGui::Checkbox(("Invert Z##" + valueName).c_str(), &invertMap)) rvm.CommitChanges();
}
*/