
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

	bool first = true;
	AABB renderableBounds;
	std::vector<RenderableMeshResourceId> renderableMeshResIds;

	for (const auto& meshDef : modelDefinition->Meshes)
	{
		// Create the Mesh resource
		auto meshId = _renderer.CreateMeshResource(meshDef); 


		// Create Texture resources and config Material
		Material mat = {};
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


		renderableMeshResIds.emplace_back(_renderer.CreateRenderableMesh(meshId, mat));

		
		// Expand bounds to contain all submeshes
		if (first)
		{
			first = false;
			renderableBounds = meshDef.Bounds;
		}
		else
		{
			renderableBounds = AABB::Merge(renderableBounds, meshDef.Bounds);
		}

		
	}

	return RenderableComponent{ renderableMeshResIds, renderableBounds };
}

TextureResourceId SceneManager::LoadTexture(const std::string& path)
{
	// Is teh tex already loaded?
	const auto it = _loadedTexturesCache.find(path);
	if (it != _loadedTexturesCache.end())
	{
		return it->second;
	}

	auto texResId = _renderer.CreateTextureResource(path);

	_loadedTexturesCache.emplace(path, texResId);

	return texResId;
}

const Material& SceneManager::GetMaterial(const RenderableMeshResourceId& resourceId) const
{
	return _renderer.GetRenderableMesh(resourceId).Mat;
}

void SceneManager::SetMaterial(const RenderableComponent& renderableComp, const Material& newMat) const
{
	for (auto& renderableMeshId : renderableComp.GetMeshIds())
	{
		_renderer.SetMaterial(renderableMeshId, newMat);
	}
}

void SceneManager::SetMaterial(const RenderableMeshResourceId& renderableResId, const Material& newMat) const
{
	_renderer.SetMaterial(renderableResId, newMat);
}

SkyboxResourceId SceneManager::LoadSkybox(const std::string& path)
{
	SkyboxResourceId skyboxResourceId;

	// See if skybox is already loaded
	const auto it = _loadedSkyboxesCache.find(path);
	if (it != _loadedSkyboxesCache.end())
	{
		// Use existing resource
		skyboxResourceId = it->second;
	}
	else
	{
		std::cout << "Creating skybox " << path << std::endl;

		// Create new resource
		const auto ids = _renderer.CreateIblTextureResources(path);
		SkyboxCreateInfo createInfo = {};
		createInfo.IblTextureIds = ids;
		skyboxResourceId = _renderer.CreateSkybox(createInfo);

		_loadedSkyboxesCache.emplace(path, skyboxResourceId);
	}

	return skyboxResourceId;
}

void SceneManager::SetSkybox(const SkyboxResourceId& id)
{
	_skybox = id;
	_renderer.SetSkybox(id);
}

SkyboxResourceId SceneManager::GetSkybox() const
{
	return _skybox;
}
