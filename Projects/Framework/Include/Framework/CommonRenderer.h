#pragma once

#include "CommonTypes.h"


// TODO Move these back to the Renderer layer

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderOptions
{
	float ExposureBias = 1;
	float SkyboxRotation = 0; // degrees
	bool ShowIrradiance = true;
	bool ShowClipping = false;
	//bool DrawDepth = false;
	//bool DrawNormals = false;
	//bool DisableShadows = false;
	//bool UseMsaa = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxIdType;
struct RenderableIdType;
struct MeshIdType;
struct TextureIdType;
//struct ShaderIdType;
typedef TypedId<SkyboxIdType> SkyboxResourceId;
typedef TypedId<RenderableIdType> RenderableResourceId;
typedef TypedId<MeshIdType> MeshResourceId;
typedef TypedId<TextureIdType> TextureResourceId;
//typedef ResourceId<ShaderIdType> ShaderResourceId;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct IblTextureResourceIds
{
	TextureResourceId EnvironmentCubemapId;
	TextureResourceId IrradianceCubemapId;
	TextureResourceId PrefilterCubemapId;
	TextureResourceId BrdfLutId;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxCreateInfo
{
	IblTextureResourceIds IblTextureIds;
};
