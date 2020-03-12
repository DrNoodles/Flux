#pragma once
#include "imgui/imgui.h"
#include <optional>

struct RenderableVm;
class TransformVm;
class LightVm;

class PropsView
{
public:
	void DrawUI(int selectionCount, 
		TransformVm& tvm, 
		std::optional<RenderableVm>& rvm, 
		std::optional<LightVm>& lvm) const;

private:
	ImVec4 _headingColor = ImVec4{ .5,.5,.5,1 };

	void DrawTransformPanel(TransformVm& tvm) const;
	void DrawLightPanel(LightVm& lvm) const;
	/*void DrawRenderablePanel(RenderableVm& rvm) const;
	
	void BaseColor(RenderableVm& rvm) const;
	void Metalness(RenderableVm& rvm) const;
	void Roughness(RenderableVm& rvm) const;
	void AmbientOcclusion(RenderableVm& rvm) const;
	void Normals(RenderableVm& rvm) const;*/
};


