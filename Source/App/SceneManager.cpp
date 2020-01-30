
#include "SceneManager.h"
#include "Entity/RenderableComponent.h"

#include <vector>
#include <unordered_map>
#include <iostream>

RenderableComponent SceneManager::LoadRenderableComponentFromFile(const std::string& path)
{
	auto modelDefinition = _modelLoaderService.LoadModel(path);
	if (!modelDefinition.has_value())
	{
		throw std::invalid_argument("Couldn't load model");
	}

	RenderableComponent rc = {};

	
	for (const auto& meshDef : modelDefinition->Meshes)
	{
		RenderableCreateInfo renderableCreateInfo = {};

		// Create the Mesh resource
		renderableCreateInfo.MeshId = _renderer.CreateMeshResource(meshDef);


		// Create Texture resources and config Material
		auto& mat = renderableCreateInfo.Mat;
		for (const auto& texDef : meshDef.Textures)
		{
			const auto texResId = LoadTexture(texDef.Path);

			switch (texDef.Type)
			{
			case TextureType::Basecolor:
				mat.UseBasecolorMap = true;
				mat.BasecolorMap = texResId;
				break;

			case TextureType::Normals:
				mat.UseNormalMap = true;
				mat.NormalMap = texResId;
				break;

			case TextureType::Roughness:
				mat.UseRoughnessMap = true;
				mat.RoughnessMap = texResId;
				break;

			case TextureType::Metalness:
				mat.UseMetalnessMap = true;
				mat.MetalnessMap = texResId;
				break;

			case TextureType::AmbientOcclusion:
				mat.UseAoMap = true;
				mat.AoMap = texResId;
				break;

			case TextureType::Undefined:
			default:
				std::cerr << "Discarding unknown texture type" << std::endl;
			}
		}

		//// Hardcoded test data TODO Remove this, duh.
		//mat.UseRoughnessMap = true;
		//mat.RoughnessMapChannel = Material::Channel::Green;
		//mat.RoughnessMap = mat.AoMap;
		//
		//mat.UseMetalnessMap = true;
		//mat.MetalnessMapChannel = Material::Channel::Blue;
		//mat.MetalnessMap = mat.AoMap;
		
		rc.RenderableId = _renderer.CreateRenderable(renderableCreateInfo);
		
		break; // TODO Only processing the first mesh for now!
	}

	return rc;
}

TextureResourceId SceneManager::LoadTexture(const std::string& path)
{
	// Is teh tex already loaded?
	const auto it = _loadedTextures.find(path);
	if (it != _loadedTextures.end())
	{
		return it->second;
	}

	auto texResId = _renderer.CreateTextureResource(path);

	_loadedTextures.emplace(path, texResId);

	return texResId;
}

const Material& SceneManager::GetMaterial(const RenderableResourceId& resourceId) const
{
	const Renderable& renderable = _renderer.GetRenderable(resourceId);
	return renderable.Mat;
}

void SceneManager::SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat)
{
	// TODO Undo redo
	_renderer.SetMaterial(renderableResId, newMat);
}
