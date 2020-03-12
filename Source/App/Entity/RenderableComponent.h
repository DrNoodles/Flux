#pragma once

#include "Renderer/GpuTypes.h"
#include "Renderer/RenderableMesh.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderableComponent
{
public:
	RenderableComponent() = delete;

	explicit RenderableComponent(std::vector<RenderableMeshResourceId> meshIds, AABB bounds)
	{
		_renderableIds = std::move(meshIds);
		_bounds = bounds;
	}
	
	explicit RenderableComponent(RenderableMeshResourceId meshId, AABB bounds)
		: RenderableComponent{ std::vector<RenderableMeshResourceId>{ meshId }, bounds }
	{
	}

	AABB GetBounds() const { return _bounds; }
	const std::vector<RenderableMeshResourceId>& GetMeshIds() const { return _renderableIds; }
	
private:
	AABB _bounds;
	std::vector<RenderableMeshResourceId> _renderableIds;

	// TODO Use this space to add additional data used for the App/Ui layer
};
