#pragma once

#include <Framework/AABB.h>
#include <Framework/CommonRenderer.h>
#include <Framework/Material.h>

#include <string>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableComponentSubmesh
{
	const RenderableResourceId Id; // TODO change to a MeshAssetId when that's a thing
	std::string Name;
	MaterialId MatId;

	RenderableComponentSubmesh() = delete;
	RenderableComponentSubmesh(RenderableResourceId id, std::string name, MaterialId mat)
		: Id(id), Name(std::move(name)), MatId(mat)
	{
	}
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderableComponent
{
public:
	RenderableComponent() = delete;

	explicit RenderableComponent(std::vector<RenderableComponentSubmesh> submeshes, AABB bounds)
	{
		_submeshes = std::move(submeshes);
		_bounds = bounds;
	}
	
	explicit RenderableComponent(RenderableComponentSubmesh submeshes, AABB bounds)
		: RenderableComponent{ std::vector<RenderableComponentSubmesh>{std::move(submeshes)}, bounds }
	{
	}

	AABB GetBounds() const { return _bounds; }
	std::vector<RenderableComponentSubmesh>& GetSubmeshes() { return _submeshes; }
	
private:
	AABB _bounds;
	std::vector<RenderableComponentSubmesh> _submeshes;

	// TODO Use this space to add additional data used for the App/Ui layer
};
