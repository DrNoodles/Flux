#pragma once

#include "Renderer/GpuTypes.h"
#include "Renderer/RenderableMesh.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableComponentSubmesh
{
	const RenderableResourceId Id;
	const std::string Name;
	RenderableComponentSubmesh() = delete;
	RenderableComponentSubmesh(RenderableResourceId id, std::string name) : Id(id), Name(std::move(name)) {}
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderableComponent
{
public:
	RenderableComponent() = delete;

	explicit RenderableComponent(std::vector<RenderableComponentSubmesh> submeshes, AABB bounds)
	{
		_renderableIds = std::move(submeshes);
		_bounds = bounds;
	}
	
	explicit RenderableComponent(RenderableComponentSubmesh submeshes, AABB bounds)
		: RenderableComponent{ std::vector<RenderableComponentSubmesh>{std::move(submeshes)}, bounds }
	{
	}

	AABB GetBounds() const { return _bounds; }
	const std::vector<RenderableComponentSubmesh>& GetSubmeshes() const { return _renderableIds; }
	
private:
	AABB _bounds;
	std::vector<RenderableComponentSubmesh> _renderableIds;

	// TODO Use this space to add additional data used for the App/Ui layer
};
