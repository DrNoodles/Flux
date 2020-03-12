#pragma once

#include "Renderer/GpuTypes.h"
#include "Renderer/RenderableMesh.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderableComponent
{
public:
	RenderableComponent() = delete;

	explicit RenderableComponent(std::vector<RenderableMeshResourceId> meshIds)
	{
		_renderableIds = std::move(meshIds);
	}
	
	explicit RenderableComponent(RenderableMeshResourceId meshId)
		: RenderableComponent{ std::vector<RenderableMeshResourceId>{ meshId} }
	{
	}

	AABB GetAABB() const { return _aabb; }
	const std::vector<RenderableMeshResourceId>& GetMeshIds() const { return _renderableIds; }
	
private:
	AABB _aabb;
	std::vector<RenderableMeshResourceId> _renderableIds;

	// TODO Use this space to add additional data used for the App/Ui layer
};
