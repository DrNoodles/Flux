#pragma once

#include "GpuTypes.h"
#include "Material.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableCreateInfo
{
	MeshResourceId Mesh;
	Material Mat;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Renderable
{
	MeshResourceId MeshId;
	Material Mat;

	// Array containing one per frame in flight
	// TODO Eventually auto generate Infos stored in an unordered map key would be the unique combination of mesh and texture resources
	std::vector<ModelResourceFrame> FrameResources{};
};
