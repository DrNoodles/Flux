#pragma once

#include "GpuTypes.h"
#include "Material.h"


// TODO Split Skybox into its own file

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxCreateInfo
{
	MeshResourceId MeshId;
	TextureResourceId TextureId;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Skybox
{
	MeshResourceId MeshId;
	TextureResourceId TextureId;

	// Array containing one per frame in flight
	std::vector<SkyboxResourceFrame> FrameResources{};
};





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableCreateInfo
{
	MeshResourceId MeshId;
	Material Mat;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Renderable
{
	MeshResourceId MeshId;
	Material Mat;

	// Array containing one per frame in flight
	// TODO Eventually auto generate Infos stored in an unordered map key would be the unique combination of mesh and texture resources
	std::vector<PbrModelResourceFrame> FrameResources{};
};
