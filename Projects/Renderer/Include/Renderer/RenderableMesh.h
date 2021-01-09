#pragma once

#include "GpuTypes.h"

#include <Framework/CommonRenderer.h>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Skybox
{
	MeshResourceId MeshId;
	IblTextureResourceIds IblTextureIds;

	//TODO Separate this from Skybox.
	std::vector<SkyboxResourceFrame> FrameResources{}; // Array containing one per frame in flight
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO Eventually auto generate Infos stored in an unordered map key would be the unique combination of mesh and texture resources

//TODO Think about this in some depth...
/*
- To make instance rendering possible the frame resources likely need to be decoupled from from RenderableMesh.
- ?? Make the PbrCommonResourceFrame descriptor set only have info that pertains to this unique MeshAsset?
- ?? Create a unique PbrCommonResourceFrame for each unique MeshAsset / Material combo and move transform into a diff per-renderable UBO. info.Some ofther abstractionConsidering MeshAsset will be unique for each MeshA
	Necessary to make instance rendering possible. The

- look at PbrModelRenderPass::CreatePbrDescriptorSetLayout()
	- dependencies are
		VS
		- transform
		FS
		- 
		- ibl images
		- material
			- images
			- properties

	- create descSet for each of
		- Each usage of a given MeshAsset (1 mesh asset could be used with many transforms)
		- Each unique Material should have a desc set.
*/
struct RenderableMesh
{
	MeshResourceId MeshId;
	std::vector<PbrCommonResourceFrame> CommonFrameResources{}; // Array containing one per frame in flight

	// TODO Remove below from here. It should be associated with a material, nothing to do with a mesh
	std::vector<PbrMaterialResourceFrame> MaterialFrameResources{}; // Array containing one per frame in flight
};
