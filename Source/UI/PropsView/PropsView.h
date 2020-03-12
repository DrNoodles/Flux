#pragma once
#include "imgui/imgui.h"

#include <optional>
#include <string>
#include <vector>

struct RenderableVm;
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
	virtual RenderableVm GetMaterialState() const = 0;
	virtual void CommitMaterialChanges(RenderableVm state) = 0;

	// Light?
};

class PropsView
{
public:
	PropsView() = delete;
	explicit PropsView(IPropsViewDelegate* delegate) : _delegate{delegate} {}
	void DrawUI(int selectionCount, TransformVm& tvm, std::optional<RenderableVm>& rvm, std::optional<LightVm>& lvm) const;

private:
	// Dependencies
	IPropsViewDelegate* _delegate = nullptr;
	
	ImVec4 _headingColor = ImVec4{ .5,.5,.5,1 };

	void DrawTransformPanel(TransformVm& tvm) const;
	void DrawLightPanel(LightVm& lvm) const;
	void DrawRenderablePanel(RenderableVm& rvm) const;
	
	void BaseColor(RenderableVm& rvm) const;
	void Metalness(RenderableVm& rvm) const;
	void Roughness(RenderableVm& rvm) const;
	void AmbientOcclusion(RenderableVm& rvm) const;
	void Normals(RenderableVm& rvm) const;
};


