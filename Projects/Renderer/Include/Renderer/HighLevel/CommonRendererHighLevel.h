#pragma once


#include "Renderer/LowLevel/GpuTypes.h"
#include "Framework/CommonTypes.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Light
{
	enum class LightType : i32
	{
		Point = 0, Directional = 1
	};
	f32 Intensity;
	glm::vec3 Color;
	glm::vec3 Pos;
	LightType Type;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SceneRendererPrimitives
{
	struct RenderableObject
	{
		RenderableResourceId RenderableId; // TODO Material and MeshAsset here in place of RenderableId
		glm::mat4 Transform;

		// TODO maybe use an index into the materials to show intent that this isn't the mat owner for now this is easier to get it working
		const Material& Material;  // i32 MaterialIndex = -1
	};

	std::set<const Material*> Materials{};
	std::vector<RenderableObject> Objects;
	std::vector<Light> Lights;
	glm::vec3 ViewPosition;
	glm::mat4 ViewMatrix;
	glm::mat4 ProjectionMatrix;
	glm::mat4 LightSpaceMatrix;
};
