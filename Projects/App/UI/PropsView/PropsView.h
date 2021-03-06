#pragma once

#include <Framework/Material.h>

#include <imgui/imgui.h>

#include <optional>
#include <string>
#include <vector>



struct MaterialViewState;
class TransformVm;
class LightVm;

class IPropsViewDelegate
{
public:
	virtual ~IPropsViewDelegate() = default;

	// Transform?
	
	// Materials list
	virtual const std::vector<std::pair<std::string, MaterialId>>& GetMaterials() = 0;
	virtual int GetSelectedMaterial() const = 0;
	virtual void SelectMaterial(int i) = 0;

	// Material editing
	virtual std::optional<MaterialViewState> GetMaterialState() = 0;
	virtual void CommitMaterialChanges(const MaterialViewState& state) = 0;

	// Light?
};

class PropsView
{
private:
	// Dependencies
	IPropsViewDelegate* _delegate = nullptr;
	
	// TODO refactor to have some control over styling - This is copied in SceneView.h
	ImVec4 _headingColor = ImVec4{ .5,.5,.5,1 };
	
public:
	PropsView() = delete;
	explicit PropsView(IPropsViewDelegate* delegate) : _delegate{delegate} {}
	void BuildUI(int selectionCount, TransformVm& tvm, std::optional<LightVm>& lvm) const;

private:
	void DrawTransformPanel(TransformVm& tvm) const;
	void DrawLightPanel(LightVm& lvm) const;
	void DrawMaterialPanel(std::optional<MaterialViewState>& mvm) const;
	
	void Basecolor(MaterialViewState& rvm) const;
	void Normals(MaterialViewState& rvm) const;
	void Metalness(MaterialViewState& rvm) const;
	void Roughness(MaterialViewState& rvm) const;
	void AmbientOcclusion(MaterialViewState& rvm) const;
	void Emissive(MaterialViewState& rvm) const;
	void Transparency(MaterialViewState& rvm) const;
};


