#pragma once
#include "imgui/imgui.h"

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
	
	// Submeshes
	virtual int GetSelectedSubMesh() const = 0;
	virtual void SelectSubMesh(int i) = 0;
	virtual const std::vector<std::string>& GetSubmeshes() = 0;

	// Material
	virtual std::optional<MaterialViewState> GetMaterialState() = 0;
	virtual void CommitMaterialChanges(const MaterialViewState& state) = 0;

	// Light?
};

class PropsView
{
public:
	PropsView() = delete;
	explicit PropsView(IPropsViewDelegate* delegate) : _delegate{delegate} {}
	void BuildUI(int selectionCount, TransformVm& tvm, std::optional<LightVm>& lvm) const;

private:
	// Dependencies
	IPropsViewDelegate* _delegate = nullptr;
	
	ImVec4 _headingColor = ImVec4{ .5,.5,.5,1 };

	void DrawTransformPanel(TransformVm& tvm) const;
	void DrawLightPanel(LightVm& lvm) const;
	void DrawRenderablePanel(MaterialViewState& rvm) const;
	
	void Basecolor(MaterialViewState& rvm) const;
	void Normals(MaterialViewState& rvm) const;
	void Metalness(MaterialViewState& rvm) const;
	void Roughness(MaterialViewState& rvm) const;
	void AmbientOcclusion(MaterialViewState& rvm) const;
	void Emissive(MaterialViewState& rvm) const;
};


