#pragma once

#include "GpuTypes.h"

#include <Framework/CommonRenderer.h>
#include <Framework/Material.h>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Skybox
{
	MeshResourceId MeshId;
	IblTextureResourceIds IblTextureIds;

	// Array containing one per frame in flight
	std::vector<SkyboxResourceFrame> FrameResources{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableMesh
{
	MeshResourceId MeshId;
	Material Mat;

	// Array containing one per frame in flight
	// TODO Eventually auto generate Infos stored in an unordered map key would be the unique combination of mesh and texture resources
	std::vector<PbrModelResourceFrame> FrameResources{};
};
