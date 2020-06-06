
#include "SceneManager.h"

#include <Framework/Material.h>
#include <Framework/CommonRenderer.h>

#include <vector>
#include <unordered_map>
#include <iostream>



std::optional<RenderableComponent> SceneManager::LoadRenderableComponentFromFile(const std::string& path)
{
	auto modelDefinition = _modelLoaderService.LoadModel(path);
	if (!modelDefinition.has_value())
	{
		std::cerr << "Failed to load renderable component from file: " << path << std::endl;
		return std::nullopt;
		//throw std::invalid_argument("Couldn't load model");
	}

	bool first = true;
	AABB renderableBounds;
	std::vector<RenderableComponentSubmesh> submeshes;

	for (const auto& meshDef : modelDefinition->Meshes)
	{
		// Create the Mesh resource
		auto meshId = _delegate.CreateMeshResource(meshDef); 


		// Create Texture resources and config Material
		Material mat = {};
		for (const auto& texDef : meshDef.Textures)
		{
			const auto texResId = LoadTexture(texDef.Path);
			if (!texResId.has_value())
			{
				// TODO User error here. Also, rework this method so unused items are unloaded from memory.
				throw std::invalid_argument("Unable to load texture");
			}
			
			auto map = Material::Map{ *texResId, texDef.Path };
			
			switch (texDef.Type)
			{
			case TextureType::Basecolor:
				mat.UseBasecolorMap = true;
				mat.BasecolorMap = std::move(map);
				break;

			case TextureType::Normals:
				//mat.UseNormalMap = true;
				mat.NormalMap = std::move(map);
				break;

			case TextureType::Roughness:
				mat.UseRoughnessMap = true;
				mat.RoughnessMap = std::move(map);
				break;

			case TextureType::Metalness:
				mat.UseMetalnessMap = true;
				mat.MetalnessMap = std::move(map);
				break;

			case TextureType::AmbientOcclusion:
				//mat.UseAoMap = true;
				mat.AoMap = std::move(map);
				break;

			case TextureType::Emissive:
				mat.EmissiveMap = std::move(map);
				break;

			case TextureType::Undefined:
			default:
				std::cerr << "Discarding unknown texture type" << std::endl;
			}
		}


		RenderableComponentSubmesh submesh = { _delegate.CreateRenderable(meshId, mat), meshDef.Name };
		submeshes.emplace_back(submesh);

		
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

	return RenderableComponent{ submeshes, renderableBounds };
}

std::optional<TextureResourceId> SceneManager::LoadTexture(const std::string& path)
{
	// Is teh tex already loaded?
	const auto it = _loadedTexturesCache.find(path);
	if (it != _loadedTexturesCache.end())
	{
		return it->second;
	}

	TextureResourceId resId;
	try
	{
		resId = _delegate.CreateTextureResource(path);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to load texture " << path << std::endl;
		return std::nullopt;
	}
	
	_loadedTexturesCache.emplace(path, resId);

	return resId;
}

const Material& SceneManager::GetMaterial(const RenderableResourceId& resourceId) const
{
	return _delegate.GetMaterial(resourceId);
}

void SceneManager::SetMaterial(const RenderableComponent& renderableComp, const Material& newMat) const
{
	for (const auto& submesh : renderableComp.GetSubmeshes())
	{
		_delegate.SetMaterial(submesh.Id, newMat);
	}
}

void SceneManager::SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat) const
{
	_delegate.SetMaterial(renderableResId, newMat);
}

SkyboxResourceId SceneManager::LoadAndSetSkybox(const std::string& path)
{
	SkyboxResourceId id;
	
	// Check if skybox is already loaded
	const auto it = _loadedSkyboxesCache.find(path);
	if (it != _loadedSkyboxesCache.end())
	{
		id = it->second;
	}
	else
	{
		// Load and cache skybox
		std::cout << "Creating skybox " << path << std::endl;

		// Create new resource
		const auto ids = _delegate.CreateIblTextureResources(path);
		SkyboxCreateInfo createInfo = {};
		createInfo.IblTextureIds = ids;
		
		id = _delegate.CreateSkybox(createInfo);

		_loadedSkyboxesCache.emplace(path, id);
	}

	SetSkybox(id);
	return id;
}

void SceneManager::SetSkybox(const SkyboxResourceId& id)
{
	_skybox = id;
	_delegate.SetSkybox(id);
}

SkyboxResourceId SceneManager::GetSkybox() const
{
	return _skybox;
}
